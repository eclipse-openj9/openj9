* Copyright (c) 2016, 2018 IBM Corp. and others

      ACONTROL AFPR,FLAG(CONT)

FFISYS EDCXPRLG DSASIZE=DSASZ,PSECT=ASP

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
          USING  CEEDSAHP,4
         L  14,0(,2)          ecif->cif
         L  14,8(,14)         cif->arg_types
*Name:    ffi_prep_args_call
*Input:   stack, extended_cif
*Output:  void
*Action:  Is an external call to C-XPLINK routine
*         It saves function's arguments in this 
*         routine's local storage
         LA 1,LSTOR
         LR 13,1
         EDCXCALL  PREPARGS,WORKREG=10

         LR 5,14              Copy of parameter types
*Reset registers used in code gen
         SR  0,0              GPR counter
         SR  14,14            FPR counter
         SR  7,7              Offset in the arg area
         SR  10,10            Offset in array of parm types
         SR  6,6              Offset in stored parm values

*Get the cif->nargs from caller's stack
         L   9,(2112+(((DSASZ+31)/32)*32)+20)(,4) 

*Place arguments passed to the foreign function based on type
ARGLOOP  L   11,0(10,5)       Get pointer to current ffi_type
         LLC 11,7(11)         ffi_type->type
         SLL 11,2             Find offset in branch tabel
         LA  15,ATABLE
         L   15,0(11,15)
         BR  15
 
*Following code prepares ffi arguments, according to xplink

I        DS  0H               ffi_type_int
UI32     DS 0H                ffi_type_uint32
         LA 15,I32
         LR 11,0
         SLL 11,2             Pass this parm in gpr or arg area
         L  15,0(11,15)
         CFI  0,3             GPRs left to pass parms in?
         BL INC
         LA 0,3               Reached max gprs
         B  J
INC      DS 0H
         AHI 0,1              Now we have one less gpr left
J        DS 0H
         BR 15

IGPR1    DS 0H                INT/UI32 type passed in gpr1
         L 1,0(6,13)
         AHI 7,4              Advance to next word in arg area
         B CONT               Next parameter
IGPR2    DS 0H                INT/UI32 type passed in gpr2
         L 2,0(6,13)
         AHI 7,4              Advance to next word in arg area
         B CONT               Next parameter
IGPR3    DS 0H                INT/UI32 type passed in gpr3
         L 3,0(6,13)
         AHI 7,4              Advance to next word in arg area
         B CONT               Next parameter
IARGA    DS 0H                INT/UI32 stored in arg area
         L 11,0(6,13)         Argument value
         LA 15,2112(,4)       Start of arg area
         ST 11,0(7,15)        Store in next word in arg area
         AHI 7,4              Bump to the next word in arg area
         B CONT               Next parameter

UI8      DS 0H                ffi_type_uint8
SI8      DS 0H                ffi_type_sint8
         LA 15,I8
         LR 11,0
         SLL 11,2             Pass this parm in gpr or arg area
         L  15,0(11,15)
         CFI  0,3             GPRs left to pass parms in?
         BL INC2
         LA 0,3               Reached max gprs
         B  J2
INC2     DS 0H
         AHI 0,1              Now we have one less gpr left
J2       DS 0H
         BR 15 

I8GPR1   DS 0H                Char type passed in gpr1
         LLC 1,0(6,13)
         AHI 7,4              Advance to next word in arg area
         B  CONT              Next parameter
I8GPR2   DS 0H                Char type passed in gpr2
         LLC 2,0(6,13)
         AHI 7,4              Advance to next word in arg area
         B CONT               Next parameter
I8GPR3   DS 0H                Char type passed in gpr3
         LLC 3,0(6,13)
         AHI 7,4              Advance to the next word in arg area
         B CONT
I8ARGA   DS 0H                Char stored in arg area
         LLC 11,0(6,13)       Argument value
         LA 15,2112(,4)       Start of arg area
         ST 11,0(7,15)       Store in next word in arg area
         AHI 7,4              Bump to the next word in arg area
         B CONT               Next parameter

UI16     DS 0H                ffi_type_uint16
         LA 15,U16
         LR 11,0
         SLL 11,2
         L  15,0(11,15)       Pass this parm in gpr or arg area
         CFI  0,3             GPRs left to pass parms in?
         BL INC3
         LA 0,3               Reached max gprs
         B  J3
INC3     DS 0H
         AHI 0,1              Now we have one less gpr left
J3       DS 0H
         BR 15 

U16GPR1  DS 0H                u_short type passed in gpr1
         LLH 1,0(6,13)
         AHI 7,4              Advance to the next word in arg area
         B CONT               Next parameter
U16GPR2  DS 0H                u_short type passed in gpr2
         LLH 2,0(6,13)
         AHI 7,4              Advance to the next word in arg area
         B CONT               Next parameter
U16GPR3  DS 0H                u_short passed in gpr3
         LLH 3,0(6,13)
         AHI 7,4              Advance to the next word in arg area
         B CONT               Next parameter
U16ARGA  DS 0H                u_short in arg area
         LLH 11,0(6,13)       Argument value
         LA 15,2112(,4)       Start of arg area
         ST 11,0(7,15)        Store in next word in arg area
         AHI 7,4              Bump to the next word in arg area
         B CONT               Next parameter

SI16     DS 0H                ffi_type_sint16
         LA 15,S16
         LR 11,0
         SLL 11,2             Pass this parm in gpr or arg area
         L  15,0(11,15)
         CFI  0,3             GPRs left to pass parms in?
         BL INC4
         LA 0,3               Reached max gprs
         B  J4
INC4     DS 0H
         AHI 0,1              Now we have one less gpr left
J4       DS 0H
         BR 15 

S16GPR1  DS 0H                s_SHORT type passsed in gpr1
         LH 1,0(6,13)
         AHI 7,4              Advance to the next word in arg area
         B CONT               Next parameter
S16GPR2  DS 0H                s_SHORT type passed in gpr2
         LH 2,0(6,13)
         AHI 7,4              Advance to the next word in arg area
         B CONT               Next parameter
S16GPR3  DS 0H                s_SHORT type passed in gpr3
         LH 3,0(6,13)
         AHI 7,4              Advance to next word in arg area
         B CONT               Next parameter
S16ARGA  DS 0H                s_SHORT in arg area
         LH 11,0(6,13)        Argument value
         LA 15,2112(,4)       Start of arg area
         ST  11,0(7,15)       Store in next word in arg area
         AHI 7,4              Bump to the next word in arg area
         B CONT               Next parameter

SI64     DS 0H                ffi_type_sint64
         LA 15,S64
         LR 11,0
         SLL 11,2             Pass this parm in gpr or arg area
         L  15,0(11,15)
         CFI  0,2             GPRs left to pass parms in?
         BL INC5
         LA 0,3               Reached max gprs
         B  J5
INC5     DS 0H
         AHI 0,2              Now we have two less gpr left
J5       DS 0H
         BR 15 

S64GP12 DS 0H                INT64 type passed in gpr1
         L  1,0(6,13)
         L  2,4(6,13)
         AHI 7,8              Advance next two words in arg area
         AHI 6,4              Advance the first 4 bytes of parm value
         B CONT               Next parameter
S64GP23 DS 0H                INT64 type passed in gpr2,gpr3
         L  2,0(6,13)
         L  3,4(6,13)
         AHI 7,8              Advance next two words in arg area
         AHI 6,4              Advance the first 4 bytes of parm value
         B CONT               Next parameter
S64GPR3  DS 0H                INT64 type passed in gpr2
         L  3,0(6,13)
         AHI 7,4              Advance next word in arg area
         L  11,4(6,13)
         LA 15,2112(,4)       Start of arg area
         ST 11,0(7,15)
         AHI 7,4              Advance next word in arg area
         AHI 6,4              Advance the first 4 bytes of parm value
         B CONT               Next parameter
S64ARGA  DS 0H                INT64 in arg area
         L  11,0(6,13)        Argument value
         LA 15,2112(,4)       Start of arg area
         ST  11,0(7,15)       Store in next word in arg area
         AHI 7,4              Bump one word in arg area
         L  11,4(6,13)        Argument value
         ST  11,0(7,15)       Store in next word in arg area
         AHI 7,4              Bump one word in arg area
         AHI 6,4              Advance the first 4 bytes of parm value
         B CONT               Next parameter

PTR      DS 0H                ffi_type_pointer
         LA 15,PTRS
         LR 11,0
         SLL 11,2             Pass this parm in gpr or arg area
         L  15,0(11,15)
         CFI  0,3             GPRs left to pass parms in?
         BL INC6
         LA 0,3               Reached max gprs
         B  J6
INC6     DS 0H
         AHI 0,1              Now we have one less gpr left
J6       DS 0H
         BR 15 

PTRG1    DS 0H                PTR type passed in gpr1
         L 1,0(6,13)
         AHI 7,4              Advance to the next word in arg area
         B CONT               Next parameter
PTRG2    DS 0H                PTR type passed in gpr2
         L 2,0(6,13)
         AHI 7,4              Advance to the next word in arg area
         B CONT               Next parameter
PTRG3    DS 0H                PTR type passed in gpr3
         L 3,0(6,13)
         AHI 7,4              Advance to the next word in arg area
         B CONT               Next parameter
PTRARG   DS 0H                PTR in arg area
         L 11,0(6,13)         Argument value
         LA 15,2112(,4)       Start of arg area
         ST 11,0(7,15)        Store in next word in arg area
         AHI 7,4              Bump to the next word in arg area
         B CONT               Next parameter

UI64     DS 0H                ffi_type_uint64
         LA 15,UI64S
         LR 11,0
         SLL 11,2             Pass this parm in gpr or arg area
         L 15,0(11,15)
         CFI 0,2              GPRs left to pass parms in?
         BL INC64
         LA 0,3               Reached max gprs
         B J64
INC64    DS 0H
         AHI 0,1
J64      DS 0H
         BR 15

U64GP12  DS 0H                u_INT64 passed in gpr1, gpr2
         LLGF 1,0(6,13)
         LLGF 2,4(6,13)
         AHI 7,8              Advance two slots in arg area
         AHI 6,4              Advance the first 4 bytes of parm value
         B CONT               Next parameter
U64GP23  DS 0H                u_INT64 passed in gpr2, gpr3
         LLGF 2,0(6,13)
         LLGF 3,4(6,13)
         AHI 7,8              Advance two slots in arg area
         AHI 6,4              Advance the first 4 bytes of parm value
         B CONT               Next parameter
U64GPR3  DS 0H                u_INT64 passed in gpr2
         LLGF 3,0(6,13)
         AHI 7,4              Advance next word in arg area
         LLGF 11,4(6,13)
         LA 15,2112(,4)       Start of arg area
         ST 11,0(7,15)
         AHI 7,4              Advance next word in arg area
         AHI 6,4              Advance the first 4 bytes of parm value
         B CONT               Next parameter
U64ARGA  DS 0H                u_INT64 in arg area
         LLGF 11,0(6,13)      Argument value
         LA 15,2112(,4)       Start of arg area
         ST  11,0(7,15)       Store in next two words in arg area
         AHI 7,4              Bump one word in arg area
         LLGF 11,4(6,13)      Argument value
         ST  11,0(7,15)       Store in next word in arg area
         AHI 7,4              Bump one word in arg area
         AHI 6,4              Advance the first 4 bytes of parm value
         B CONT               Next parameter
   
FLT      DS 0H                ffi_type_float
         LA 15,FLTS
         LR 11,14
         SLL 11,2             Pass in fpr or arg area
         L 15,0(11,15)
         CFI 14,4             FPRs left to pass parms in?
         BL INC7L
         LA 14,4              Reached max fprs
         B JL7
INC7L    DS 0H
         AHI 14,1             Now we have one less fpr left
         CFI 0,3
         BL INCGF
         LA 0,3
         B JL7
INCGF    DS 0H
         AHI 0,1         
JL7      DS 0H
         BR 15

FLTR0    DS 0H                FLOAT passed in fpr0
         LE 0,0(6,13)
         LER 11,0
         B FLTSTR
FLTR2    DS 0H                FLOAT passed in fpr2
         LE 2,0(6,13)
         LER 11,2
         B FLTSTR
FLTR4    DS 0H                FLOAT passed in fpr4
         LE 4,0(6,13)
         LER 11,4
         B FLTSTR
FLTR6    DS 0H                FLOAT passed in fpr6
         LE 6,0(6,13)
         LER 11,6
         B FLTSTR
ARGAFT   DS 0H                FLOAT in arg area
         LE 11,0(6,13)        Argument value
FLTSTR   DS 0H
         LA 15,2112(,4)       Start of arg area
         STE 11,0(7,15)       Store in next word in arg area
         AHI 7,4              Bump to the next word in arg area
         B CONT               Next parameter

D        DS 0H                ffi_type_double
         LA 15,FLD
         LR 11,14
         SLL 11,2             Pass in fpr or arg area
         L  15,0(11,15)
         CFI  14,4            FPRs left to pass parms in?
         BL INC7
         LA 14,4              Reached max fprs
         B  J7
INC7     DS 0H
         AHI 14,1             Now we have one less fpr left
         CFI 0,2              Reached max gprs
         BL INCGD
         LA 0,3
         B J7
INCGD    DS 0H
         AHI 0,2
J7       DS 0H
         BR 15 

FLDR0    DS 0H                DOUBLE passed in fpr0
         LD 0,0(6,13)
         LDR 11,0
         B DBLSTR
FLDR2    DS 0H                DOUBLE passed in fpr2
         LD 2,0(6,13)
         LDR 11,2
         B DBLSTR
FLDR4    DS 0H                DOUBLE passed in fpr4
         LD 4,0(6,13)
         LDR 11,4
         B DBLSTR
FLDR6    DS 0H                DOUBLED passed in fpr6
         LD 6,0(6,13)
         LDR 11,6
         B DBLSTR
ARGAFL   DS 0H                DOUBLE in arg area
         LD 11,0(6,13)        Argument value
DBLSTR   DS 0H
         LA 15,2112(,4)       Start of arg area
         STD 11,0(7,15)       Store in next two words in arg area
         AHI 7,8              Bump to the next two words in arg area
         AHI 6,4              Bump stack extra time
         B CONT               Next parameter

LD       DS 0H                ffi_type_longdouble
         LA 15,LDD
         SR  11,11
         CFI 14,0             All linkage fprs are available
         BE  0(11,15)         Pass l_DOUBLE in fpr0-fpr2
         LA  11,8
         CFI 14,4
         BH  0(11,15)         Pass l_DOUBLE in arg area
         LA 11,4
         L  15,0(11,15)       Pass l_DOUBLE in fpr4-fpr6
         BR 15 

DFPR0    DS 0H                l_DOUBLE passed in fpr0-fpr2
         LD 0,0(6,13)
         LA 15,2112(,4)       Start of arg area
         STD 0,0(7,15)        Store the first 8bytes in arg area
         AHI 6,8              Bump stack to next parameter
         LD 2,0(6,13)         l_DOUBLE passed in fpr0-fpr2
         STD 6,8(7,15)
         AHI 6,4              Bump stack once here, once at end
         AHI 7,16             Advance two slots
         AHI 14,2             Now we have two less fprs left
         B CONT               Next Parameter
DFPR4    DS 0H                l_DOUBLE passed in fpr4-fpr6
         LD 4,0(6,13)
         AHI 6,8              Bump stack to next parameter
         LA 15,2112(,4)       Start of arg area
         STD 4,0(7,15)        Store the first 8bytes in arg area
         LD 6,0(6,13)         l_DOUBLE passed in fpr4-fpr6
         STD 6,8(7,15)
         AHI 6,4              Bump stack once here, once at end
         AHI 7,16             Advance two slots
         AHI 14,2             Now we have two less fprs left
         B CONT               Next parameter
DARGF    DS 0H                l_DOUBLE in arg area
         LD 11,0(6,13)        Argument value
         LA 15,2112(,4)       Start of arg area
         STD 11,0(7,15)       Store the first 8bytes in arg area
         AHI 6,8              Bump stack to next parameter
         LD 11,0(6,13)        Argument value next 8bytes
         STD 11,8(7,15)       Store the second 8bytes in arg area
         AHI 7,16             Advance two slots
         LA  14,4             We reached max fprs
         B CONT               Next parameter

*If we have spare gprs, pass up to 12bytes
*in GPRs. 
*TODO: Store left over struct to argument area
*
STRCT    DS 0H
         L   WRKREG,(2112+(((DSASZ+31)/32)*32)+24)(,4)
         CFI WRKREG,12
         BL STRCT2
         B CONT
STRCT2   DS 0H
         LA 15,STRCTS
         LR WRKREG,GPX
         SLL WRKREG,2
         L  15,0(WRKREG,15)
         BR 15

BYTE4    DS 0H
         L 1,0(6,STKREG)
         AHI GPX,1
         B CONT

BYTE8    DS 0H
         L 1,0(6,STKREG)
         L 2,4(6,STKREG)
         AHI GPX,2
         AHI 6,4
         B CONT

BYTE12   DS 0H
         L 1,0(6,STKREG)
         L 2,4(6,STKREG)
         L 3,8(6,STKREG)
         LA GPX,3
         AHI 6,8
         B CONT

CONT     DS 0H                End of processing curr_param
         AHI 10,4             Next parameter type
         AHI 6,4              Next parameter value stored
         BCT 9,ARGLOOP
  
*Get function address, first argument passed,
*and return type, third argument passed from caller's
*argument area.

         SR 11,11
         L 6,(2112+(((DSASZ+31)/32)*32))(,4)
         L 11,(2112+(((DSASZ+31)/32)*32)+8)(,4)
  
* Load environment ptr and func ptr
         L 5,16(,6)
         L 6,20(,6)

*What: Call function with call site descriptor based
*      function return type. Then process the return value
         LA 15,RTABLE
         SLL 11,2             Return type
         L  15,0(11,15)
         BR 15

RF       DS 0H
         BASR 7,6
         DC X'4700',Y((CDESCRF-(*-8))/8)

         L 7,(2112+(((DSASZ+31)/32)*32)+12)(,4)
         STE 0,0(,7)
         B RET

RD       DS 0H
         BASR 7,6
         DC X'4700',Y((CDESCRD-(*-8))/8)

         L 7,(2112+(((DSASZ+31)/32)*32)+12)(,4)
         STD 0,0(,7)
         B RET

RLD      DS 0H
         BASR 7,6
         DC X'4700',Y((CDESCRLD-(*-8))/8)
         L 7,(2112+(((DSASZ+31)/32)*32)+12)(,4)
         STD 0,0(,7)
         STD 2,8(,7)
         B RET

RI       DS 0H
         BASR 7,6
         DC X'4700',Y((CDESCRI-(*-8))/8)
         L 7,(2112+(((DSASZ+31)/32)*32)+12)(,4)
         ST 3,0(,7)
         B RET

RLL      DS 0H
         BASR 7,6
         DC X'4700',Y((CDESCRLL-(*-8))/8)
         L 7,(2112+(((DSASZ+31)/32)*32)+12)(,4)
         STM 2,3,0(7)
         B RET

* TODO: Struct return actually needs a different call descriptor
RV       DS 0H
RS       DS 0H
         BASR 7,6
         DC X'4700',Y((CDESCRV-(*-8))/8)
         B RET


RET      DS 0H 
         EDCXEPLG

ATABLE DC A(I)                Labels for parm types
 DC A(D)       
 DC A(LD)
 DC A(UI8)
 DC A(SI8)
 DC A(UI16)
 DC A(SI16)
 DC A(UI32)
 DC A(SI64)
 DC A(PTR) 
 DC A(UI64)
 DC A(FLT)
 DC A(STRCT)
 DC A(0)
 DC A(I)
 DC A(D)
I32 DC A(IGPR1)               Labels for passing INT in gpr
 DC A(IGPR2)
 DC A(IGPR3)
 DC A(IARGA)                  Label to store INT in arg area
I8 DC A(I8GPR1)               Labels for passing CHAR in gpr
 DC A(I8GPR2)
 DC A(I8GPR3)
 DC A(I8ARGA)                 Label to store CHAR in arg area
U16 DC A(U16GPR1)             Labels for passing u_SHORT in gpr
 DC A(U16GPR2)
 DC A(U16GPR3)
 DC A(U16ARGA)                Label to store u_SHORT in arg area
S16 DC A(S16GPR1)             Labels for passing s_SHORT in gpr
 DC A(S16GPR2)
 DC A(S16GPR3)
 DC A(S16ARGA)                Label to store s_SHORT in arg area
S64 DC A(S64GP12)             Labels to store INT64 in gpr
 DC A(S64GP23)
 DC A(S64GPR3)
 DC A(S64ARGA)                Label to store INT64 in arg area
PTRS DC A(PTRG1)              Labels to store PTR in gpr
 DC A(PTRG2)
 DC A(PTRG3)
 DC A(PTRARG)                 Label to store PTR in arg area
FLD DC A(FLDR0)               Labels to store DOUBLE in fpr
 DC A(FLDR2)
 DC A(FLDR4)
 DC A(FLDR6)
 DC A(ARGAFL)                 Label to store DOUBLE in arg area
LDD DC A(DFPR0)               Labels to store l_DOUBLE in fpr
 DC A(DFPR4)
 DC A(DARGF)                  Label to store l_DOUBLE in arg area
UI64S DC A(U64GP12)           Labels to store u_INT64 in gpr
  DC A(U64GPR3)
  DC A(U64ARGA)               Label to store u_INT64 in arg area
FLTS DC A(FLTR0)              Labels to store FLOAT in fpr
  DC A(FLTR2)
  DC A(FLTR4)
  DC A(FLTR6)
  DC A(ARGAFT)                Label to store FLOAT in arg area
STRCTS DC A(BYTE4)
  DC A(BYTE8)
  DC A(BYTE12)
RTABLE DC A(RV)
 DC A(RS)
 DC A(RF)
 DC A(RD)
 DC A(RLD)
 DC A(RI)
 DC A(RLL)


* Begin Call Descriptors
CDESCRV    DS 0D    Call decriptor for void return
           DC A(FFISYS#C-*)
           DC BL.3'000',BL.5'00000'
           DC BL.6'001000',BL.6'000000',BL.6'000000',BL.6'000000'
CDESCRF    DS 0D    Call decriptor for float return
           DC A(FFISYS#C-*)
           DC BL.3'000',BL.5'01000'
           DC BL.6'001000',BL.6'000000',BL.6'000000',BL.6'000000'
CDESCRD    DS 0D    Call decriptor for double return
           DC A(FFISYS#C-*)
           DC BL.3'000',BL.5'01001'
           DC BL.6'001000',BL.6'000000',BL.6'000000',BL.6'000000'
CDESCRLD   DS 0D    Call decriptor for long double return
           DC A(FFISYS#C-*)
           DC BL.3'000',BL.5'01010'
           DC BL.6'001000',BL.6'000000',BL.6'000000',BL.6'000000'
CDESCRI    DS 0D    Call decriptor for int return
           DC A(FFISYS#C-*)
           DC BL.3'000',BL.5'00001'
           DC BL.6'001000',BL.6'000000',BL.6'000000',BL.6'000000'
CDESCRLL   DS 0D    Call decriptor for long long return
           DC A(FFISYS#C-*)
           DC BL.3'000',BL.5'00010'
           DC BL.6'001000',BL.6'000000',BL.6'000000',BL.6'000000'
RETVOID  EQU  X'0'
RETSTRT  EQU  X'1'
RETFLOT  EQU  X'2'
RETDBLE  EQU  X'3'
RETINT3  EQU  X'4'
RETINT6  EQU  X'5'
OFFSET   EQU  7
BASEREG  EQU  8
GPX      EQU  0
WRKREG   EQU  11    
IDXREG   EQU  10    
STKREG   EQU  13
FPX      EQU  14
CEEDSAHP CEEDSA SECTYPE=XPLINK
ARGSL DS  XL800
LSTOR DS  XL800
DSASZ    EQU (*-CEEDSAHP_FIXED)
 END FFISYS
