/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef J9_ILOPCODES_ENUM_INCL
#define J9_ILOPCODES_ENUM_INCL

#include "il/OMRILOpCodesEnum.hpp"

   FirstJ9Op = LastOMROp + 1,

   dfconst = FirstJ9Op,   // load decimal float constant (Decimal32)
   ddconst,               // load decimal double constant (Decimal64)
   deconst,               // load decimal long double constant (Decimal128)
   dfload,                // load decimal double
   ddload,                // load decimal double
   deload,                // load decimal long double
   dfloadi,               // indirect load decimal double
   ddloadi,               // indirect load decimal double
   deloadi,               // indirect load decimal long double
   dfstore,               // store decimal float
   ddstore,               // store decimal double
   destore,               // store decimal long double
   dfstorei,              // indirect store decimal float
   ddstorei,              // indirect store decimal double
   destorei,              // indirect store decimal long double
   dfreturn,              // return a decimal float
   ddreturn,              // return a decimal double
   dereturn,              // return a decimal long double
   dfcall,                // direct call returning decimal float
   ddcall,                // direct call returning decimal double
   decall,                // direct call returning decimal long double
   dfcalli,               // indirect call returning decimal float (child1 is addr of function)
   ddcalli,               // indirect call returning decimal double (child1 is addr of function)
   decalli,               // indirect call returning decimal long double (child1 is addr of function)
   dfadd,                 // add 2 decimal doubles
   ddadd,                 // add 2 decimal doubles
   deadd,                 // add 2 decimal long doubles
   dfsub,                 // subtract 2 decimal float                        (child1 - child2)
   ddsub,                 // subtract 2 decimal doubles                      (child1 - child2)
   desub,                 // subtract 2 decimal long doubles                 (child1 - child2)
   dfmul,                 // multiply 2 decimal float
   ddmul,                 // multiply 2 decimal doubles
   demul,                 // multiply 2 decimal long doubles
   dfdiv,                 // divide 2 decimal float                          (child1 / child2)
   dddiv,                 // divide 2 decimal doubles                        (child1 / child2)
   dediv,                 // divide 2 decimal long doubles                   (child1 / child2)
   dfrem,                 // remainder of 2 decimal float                    (child1 % child2)
   ddrem,                 // remainder of 2 decimal doubles                  (child1 % child2)
   derem,                 // remainder of 2 decimal long doubles             (child1 % child2)
   dfneg,                 // negate a decimal float
   ddneg,                 // negate a decimal double
   deneg,                 // negate a decimal long double
   dfabs,                 // absolute value of decimal float
   ddabs,                 // absolute value of decimal double
   deabs,                 // absolute value of decimal long double
   dfshl,                 // shift decimal float left
   dfshr,                 // shift decimal float right
   ddshl,                 // shift decimal double left
   ddshr,                 // shift decimal double right
   deshl,                 // shift decimal long double left
   deshr,                 // shift decimal long double right
   dfshrRounded,          // shift decimal float right with rounding
   ddshrRounded,          // shift decimal float right with rounding
   deshrRounded,          // shift decimal float right with rounding
   dfSetNegative,         // set a decimal float's sign to negative
   ddSetNegative,         // set a decimal double's sign to negative
   deSetNegative,         // set a decimal long double's sign to negative
   dfModifyPrecision,     // decimal float modify precision
   ddModifyPrecision,     // decimal double modify precision
   deModifyPrecision,     // decimal long double modify precision

   i2df,        // convert integer to decimal float
   iu2df,       // convert unsigned integer to decimal float
   l2df,        // convert long integer to decimal float
   lu2df,       // convert unsigned long integer to decimal float
   f2df,        // convert float to decimal float
   d2df,        // convert double to decimal float
   dd2df,       // convert decimal double to decimal float
   de2df,       // convert decimal long double to decimal float
   b2df,        // convert byte to decimal float
   bu2df,       // convert unsigned byte to decimal float
   s2df,        // convert short to decimal float
   su2df,       // convert char to decimal float

   df2i,        // convert decimal float to integer
   df2iu,       // convert decimal float to unsigned integer
   df2l,        // convert decimal float to long integer
   df2lu,       // convert decimal float to unsigned long integer
   df2f,        // convert decimal float to float
   df2d,        // convert decimal float to double
   df2dd,       // convert decimal float to decimal double
   df2de,       // convert decimal float to decimal long double
   df2b,        // convert decimal float to byte
   df2bu,       // convert decimal float to unsigned byte
   df2s,        // convert decimal float to short integer
   df2c,        // convert decimal float to char

   i2dd,        // convert integer to decimal double
   iu2dd,       // convert unsigned integer to decimal double
   l2dd,        // convert long integer to decimal double
   lu2dd,       // convert unsigned long integer to decimal double
   f2dd,        // convert float to decimal double
   d2dd,        // convert double to decimal double
   de2dd,       // convert decimal long double to decimal double
   b2dd,        // convert byte to decimal double
   bu2dd,       // convert unsigned byte to decimal double
   s2dd,        // convert short to decimal double
   su2dd,       // convert char to decimal double

   dd2i,        // convert decimal double to integer
   dd2iu,       // convert decimal double to unsigned integer
   dd2l,        // convert decimal double to long integer
   dd2lu,       // convert decimal double to unsigned long integer
   dd2f,        // convert decimal double to float
   dd2d,        // convert decimal double to double
   dd2de,       // convert decimal double to decimal long double
   dd2b,        // convert decimal double to byte
   dd2bu,       // convert decimal double to unsigned byte
   dd2s,        // convert decimal double to short integer
   dd2c,        // convert decimal double to char

   i2de,        // convert integer to decimal long double
   iu2de,       // convert unsigned integer to decimal long double
   l2de,        // convert long integer to decimal long double
   lu2de,       // convert unsigned long integer to decimal long double
   f2de,        // convert float to decimal long double
   d2de,        // convert double to decimal long double
   b2de,        // convert byte to decimal long double
   bu2de,       // convert unsigned byte to decimal long double
   s2de,        // convert short to decimal long double
   su2de,       // convert char to decimal long double

   de2i,        // convert decimal long double to integer
   de2iu,       // convert decimal long double to unsigned integer
   de2l,        // convert decimal long double to long integer
   de2lu,       // convert decimal long double to unsigned long integer
   de2f,        // convert decimal long double to float
   de2d,        // convert decimal long double to double
   de2b,        // convert decimal long double to byte
   de2bu,       // convert decimal long double to unsigned byte
   de2s,        // convert decimal long double to short integer
   de2c,        // convert decimal long double to char

   ifdfcmpeq,   // decimal float compare and branch if equal
   ifdfcmpne,   // decimal float compare and branch if not equal
   ifdfcmplt,   // decimal float compare and branch if less than
   ifdfcmpge,   // decimal float compare and branch if greater than or equal
   ifdfcmpgt,   // decimal float compare and branch if greater than
   ifdfcmple,   // decimal float compare and branch if less than or equal
   ifdfcmpequ,  // decimal float compare and branch if equal or unordered
   ifdfcmpneu,  // decimal float compare and branch if not equal or unordered
   ifdfcmpltu,  // decimal float compare and branch if less than or unordered
   ifdfcmpgeu,  // decimal float compare and branch if greater than or equal or unordered
   ifdfcmpgtu,  // decimal float compare and branch if greater than or unordered
   ifdfcmpleu,  // decimal float compare and branch if less than or equal or unordered

   dfcmpeq,     // decimal float compare if equal
   dfcmpne,     // decimal float compare if not equal
   dfcmplt,     // decimal float compare if less than
   dfcmpge,     // decimal float compare if greater than or equal
   dfcmpgt,     // decimal float compare if greater than
   dfcmple,     // decimal float compare if less than or equal
   dfcmpequ,    // decimal float compare if equal or unordered
   dfcmpneu,    // decimal float compare if not equal or unordered
   dfcmpltu,    // decimal float compare if less than or unordered
   dfcmpgeu,    // decimal float compare if greater than or equal or unordered
   dfcmpgtu,    // decimal float compare if greater than or unordered
   dfcmpleu,    // decimal float compare if less than or equal or unordered

   ifddcmpeq,   // decimal double compare and branch if equal
   ifddcmpne,   // decimal double compare and branch if not equal
   ifddcmplt,   // decimal double compare and branch if less than
   ifddcmpge,   // decimal double compare and branch if greater than or equal
   ifddcmpgt,   // decimal double compare and branch if greater than
   ifddcmple,   // decimal double compare and branch if less than or equal
   ifddcmpequ,  // decimal double compare and branch if equal or unordered
   ifddcmpneu,  // decimal double compare and branch if not equal or unordered
   ifddcmpltu,  // decimal double compare and branch if less than or unordered
   ifddcmpgeu,  // decimal double compare and branch if greater than or equal or unordered
   ifddcmpgtu,  // decimal double compare and branch if greater than or unordered
   ifddcmpleu,  // decimal double compare and branch if less than or equal or unordered

   ddcmpeq,     // decimal double compare if equal
   ddcmpne,     // decimal double compare if not equal
   ddcmplt,     // decimal double compare if less than
   ddcmpge,     // decimal double compare if greater than or equal
   ddcmpgt,     // decimal double compare if greater than
   ddcmple,     // decimal double compare if less than or equal
   ddcmpequ,    // decimal double compare if equal or unordered
   ddcmpneu,    // decimal double compare if not equal or unordered
   ddcmpltu,    // decimal double compare if less than or unordered
   ddcmpgeu,    // decimal double compare if greater than or equal or unordered
   ddcmpgtu,    // decimal double compare if greater than or unordered
   ddcmpleu,    // decimal double compare if less than or equal or unordered

   ifdecmpeq,   // decimal long double compare and branch if equal
   ifdecmpne,   // decimal long double compare and branch if not equal
   ifdecmplt,   // decimal long double compare and branch if less than
   ifdecmpge,   // decimal long double compare and branch if greater than or equal
   ifdecmpgt,   // decimal long double compare and branch if greater than
   ifdecmple,   // decimal long double compare and branch if less than or equal
   ifdecmpequ,  // decimal long double compare and branch if equal or unordered
   ifdecmpneu,  // decimal long double compare and branch if not equal or unordered
   ifdecmpltu,  // decimal long double compare and branch if less than or unordered
   ifdecmpgeu,  // decimal long double compare and branch if greater than or equal or unordered
   ifdecmpgtu,  // decimal long double compare and branch if greater than or unordered
   ifdecmpleu,  // decimal long double compare and branch if less than or equal or unordered

   decmpeq,     // decimal long double compare if equal
   decmpne,     // decimal long double compare if not equal
   decmplt,     // decimal long double compare if less than
   decmpge,     // decimal long double compare if greater than or equal
   decmpgt,     // decimal long double compare if greater than
   decmple,     // decimal long double compare if less than or equal
   decmpequ,    // decimal long double compare if equal or unordered
   decmpneu,    // decimal long double compare if not equal or unordered
   decmpltu,    // decimal long double compare if less than or unordered
   decmpgeu,    // decimal long double compare if greater than or equal or unordered
   decmpgtu,    // decimal long double compare if greater than or unordered
   decmpleu,    // decimal long double compare if less than or equal or unordered

   dfRegLoad,   // Load decimal float global register
   ddRegLoad,   // Load decimal double global register
   deRegLoad,   // Load decimal long double global register
   dfRegStore,  // Store decimal float global register
   ddRegStore,  // Store decimal double global register
   deRegStore,  // Store decimal long double global register

   dfternary,   // Ternary Operator:  Based on the result of the first child, take the value of the
                // second or the third child.  Analogous to the "condition ? a : b" operations in C/Java.
   ddternary,   //
   deternary,   //

   dfexp,       // decimal float exponent
   ddexp,       // decimal double exponent
   deexp,       // decimal long double exponent
   dfnint,      // round decimal float to nearest int
   ddnint,      // round decimal double to nearest int
   denint,      // round decimal long double to nearest int
   dfsqrt,      // square root of decimal float
   ddsqrt,      // square root of decimal double
   desqrt,      // square root of decimal long double

   dfcos,       // cos of decimal float, returning decimal float
   ddcos,       // cos of decimal double, returning decimal double
   decos,       // cos of decimal long double, returning decimal long double
   dfsin,       // sin of decimal float, returning decimal float
   ddsin,       // sin of decimal double, returning decimal double
   desin,       // sin of decimal long double, returning decimal long double
   dftan,       // tan of decimal float, returning decimal float
   ddtan,       // tan of decimal double, returning decimal double
   detan,       // tan of decimal long double, returning decimal long double

   dfcosh,      // cosh of decimal float, returning decimal float
   ddcosh,      // cosh of decimal double, returning decimal double
   decosh,      // cosh of decimal long double, returning decimal long double
   dfsinh,      // sinh of decimal float, returning decimal float
   ddsinh,      // sinh of decimal double, returning decimal double
   desinh,      // sinh of decimal long double, returning decimal long double
   dftanh,      // tanh of decimal float, returning decimal float
   ddtanh,      // tanh of decimal double, returning decimal double
   detanh,      // tanh of decimal long double, returning decimal long double

   dfacos,      // arccos of decimal float , returning decimal float
   ddacos,      // arccos of decimal double, returning decimal double
   deacos,      // arccos of decimal long double, returning decimal long double
   dfasin,      // arcsin of decimal float , returning decimal float
   ddasin,      // arcsin of decimal double, returning decimal double
   deasin,      // arcsin of decimal long double, returning decimal long double
   dfatan,      // arctan of decimal float , returning decimal float
   ddatan,      // arctan of decimal double, returning decimal double
   deatan,      // arctan of decimal long double, returning decimal long double

   dfatan2,     // arctan2 of decimal float , returning decimal float
   ddatan2,     // arctan2 of decimal double, returning decimal double
   deatan2,     // arctan2 of decimal long double, returning decimal long double
   dflog,       // log of decimal float , returning decimal float
   ddlog,       // log of decimal double, returning decimal double
   delog,       // log of decimal long double, returning decimal long double
   dffloor,     // floor of decimal float , returning decimal float
   ddfloor,     // floor of decimal double, returning decimal double
   defloor,     // floor of decimal long double, returning decimal long double
   dfceil,      // ceil of decimal float , returning decimal float
   ddceil,      // ceil of decimal double, returning decimal double
   deceil,      // ceil of decimal long double, returning decimal long double
   dfmax,       // max of decimal float , returning decimal float
   ddmax,       // max of decimal double, returning decimal double
   demax,       // max of decimal long double, returning decimal long double
   dfmin,       // min of decimal float , returning decimal float
   ddmin,       // min of decimal double, returning decimal double
   demin,       // min of decimal long double, returning decimal long double

   dfInsExp,    // decimal float insert exponent
   ddInsExp,    // decimal double insert exponent
   deInsExp,    // decimal long double insert exponent
   
   ddclean,
   declean,

   zdload,      // zoned decimal load
   zdloadi,     // indirect zoned decimal load
   zdstore,     // zoned decimal store
   zdstorei,    // indirect zoned decimal store

   pd2zd,       // packed decimal to zoned decimal (UNPK)
   zd2pd,       // zoned decimal to packed decimal (PACK)

   zdsleLoad,   // zoned decimal sign leading embedded load
   zdslsLoad,   // zoned decimal sign leading separate load
   zdstsLoad,   // zoned decimal sign leading separate load

   zdsleLoadi,  // indirect zoned decimal sign leading embedded load
   zdslsLoadi,  // indirect zoned decimal sign leading separate load
   zdstsLoadi,  // indirect zoned decimal sign trailing separate load

   zdsleStore,  // zoned decimal sign leading embedded store
   zdslsStore,  // zoned decimal sign leading separate store
   zdstsStore,  // zoned decimal sign trailing separate store

   zdsleStorei, // indirect zoned decimal sign leading embedded store
   zdslsStorei, // indirect zoned decimal sign leading separate store
   zdstsStorei, // indirect zoned decimal sign trailing separate store

   zd2zdsle,    // zoned decimal to zoned decimal sign leading embedded
   zd2zdsls,    // zoned decimal to zoned decimal sign leading separate
   zd2zdsts,    // zoned decimal to zoned decimal sign trailing separate

   zdsle2pd,    // zoned decimal sign leading embedded to packed decimal
   zdsls2pd,    // zoned decimal sign leading separate to packed decimal
   zdsts2pd,    // zoned decimal sign trailing separate to packed decimal

   zdsle2zd,    // zoned decimal sign leading embedded to zoned decimal
   zdsls2zd,    // zoned decimal sign leading separate to zoned decimal
   zdsts2zd,    // zoned decimal sign trailing separate to zoned decimal

   pd2zdsls,            // packed decimal to zoned decimal sign leading separate
   pd2zdslsSetSign,     // packed decimal to zoned decimal sign leading separate with forced sign code setting
   pd2zdsts,            // packed decimal to zoned decimal sign trailing separate
   pd2zdstsSetSign,     // packed decimal to zoned decimal sign trailing separate with forced sign code setting

   zd2df,       // zoned decimal to decimal float
   df2zd,       // decimal float to zoned decimal
   zd2dd,       // zoned decimal to decimal double
   dd2zd,       // decimal double to zoned decimal
   zd2de,       // zoned decimal to decimal long double
   de2zd,       // decimal long double to zoned decimal

   zd2dfAbs,         // zoned decimal to decimal float abs value
   zd2ddAbs,         // zoned decimal to decimal double abs value
   zd2deAbs,         // zoned decimal to decimal long double abs value

   df2zdSetSign,     // decimal float to zoned decimal with forced sign code setting
   dd2zdSetSign,     // decimal double to zoned decimal with forced sign code setting
   de2zdSetSign,     // decimal long double to zoned decimal with forced sign code setting

   df2zdClean,       // decimal float to zoned decimal with forced sign cleaning
   dd2zdClean,       // decimal double to zoned decimal with forced sign cleaning
   de2zdClean,       // decimal long double to zoned decimal with forced sign cleaning

   udLoad,      // unicode decimal load
   udslLoad,    // unicode decimal sign leading load
   udstLoad,    // unicode decimal sign trailing load

   udLoadi,     // indirect unicode decimal load
   udslLoadi,   // indirect unicode decimal sign leading load
   udstLoadi,   // indirect unicode decimal sign trailing load

   udStore,     // unicode decimal store
   udslStore,   // unicode decimal sign leading store
   udstStore,   // unicode decimal sign trailing store

   udStorei,    // indirect unicode decimal store
   udslStorei,  // indirect unicode decimal sign leading store
   udstStorei,  // indirect unicode decimal sign trailing store

   pd2ud,       // packed decimal to unicode decimal
   pd2udsl,     // packed decimal to unicode decimal sign leading
   pd2udst,     // packed decimal to unicode decimal sign trailing

   udsl2ud,     // unicode decimal sign leading to unicode decimal
   udst2ud,     // unicode decimal sign trailing to unicode decimal

   ud2pd,       // unicode decimal to packed decimal
   udsl2pd,     // unicode decimal sign leading to packed decimal
   udst2pd,     // unicode decimal sign trailing to packed decimal

   pdload,     // packed decimal load
   pdloadi,    // indirect packed decimal load
   pdstore,    // packed decimal store
   pdstorei,   // indirect packed decimal store
   pdadd,      // packed decimal add
   pdsub,      // packed decimal subtract
   pdmul,      // packed decimal multiply
   pddiv,      // packed decimal divide
   pdrem,      // packed decimal remainder
   pdneg,      // packed decimal negation
   pdabs,      // packed decimal absolute value
   pdshr,      // packed decimal shift right
   pdshl,      // packed decimal shift left

   pdshrSetSign,        // packed decimal shift right and set the sign code
   pdshlSetSign,        // packed decimal shift left and set the sign code
   pdshlOverflow,       // packed decimal shift left with overflow detection
   pdchk,               // packed decimal validity checking

   pd2i,       // packed decimal to signed integer (SINT32)
   pd2iOverflow, // packed decimal to signed integer with overflow on (SINT32)
   pd2iu,      // packed decimal to unsigned integer (UINT32)
   i2pd,       // signed integer (SINT32) to packed decimal
   iu2pd,      // unsigned integer (UINT32) to packed decimal
   pd2l,       // packed decimal to signed long (SINT64)
   pd2lOverflow, // packed decimal to signed integer with overflow on (SINT64)
   pd2lu,      // packed decimal to unsigned long (UINT64)
   l2pd,       // signed long (SINT64) to packed decimal
   lu2pd,      // unsigned long (UINT64) to packed decimal
   pd2f,       // packed decimal to float
   pd2d,       // packed decimal to double
   f2pd,       // float to packed decimal
   d2pd,       // double to packed decimal

   pdcmpeq,   // packed decimal compare if equal
   pdcmpne,   // packed decimal compare if not equal
   pdcmplt,   // packed decimal compare if less than
   pdcmpge,   // packed decimal compare if greater than or equal
   pdcmpgt,   // packed decimal compare if greater than
   pdcmple,   // packed decimal compare if less than or equal

   pdclean,       // set the sign code to the preferred sign code and force 0 to the positive sign code
   pdclear,          // clear the specified range of packed decimal digits
   pdclearSetSign,   // clear the specified range of packed decimal digits and set the sign to the specified value

   pdSetSign,      // packed decimal forced sign code setting

   pdModifyPrecision,      // packed decimal modify precision

   countDigits, // Inline code for counting digits of an integer/long value

   pd2df,       // packed decimal to decimal float
   pd2dfAbs,       // packed decimal to decimal float abs value
   df2pd,       // decimal float to packed decimal
   df2pdSetSign,// decimal float to packed decimal, setting the sign
   df2pdClean,  // decimal float to packed decimal with forced sign cleaning
   pd2dd,       // packed decimal to decimal double
   pd2ddAbs,       // packed decimal to decimal double abs value
   dd2pd,       // decimal double to packed decimal
   dd2pdSetSign,// decimal double to packed decimal, setting the sign
   dd2pdClean,  // decimal double to packed decimal with forced sign cleaning
   pd2de,       // packed decimal to decimal long double
   pd2deAbs,       // packed decimal to decimal long double abs value
   de2pd,       // decimal long double to packed decimal
   de2pdSetSign,// decimal long double to packed decimal, setting the sign
   de2pdClean,  // decimal long double to packed decimal with forced sign cleaning
   BCDCHK,    
   ircload,     // reverse load [opposite endian to natural platform endian] of an unsigned 2 byte value
   irsload,     // reverse load [opposite endian to natural platform endian] of a signed 2 byte value
   iruiload,    // reverse load [opposite endian to natural platform endian] of an unsigned 4 byte value
   iriload,     // reverse load [opposite endian to natural platform endian] of a signed 4 byte value
   irulload,    // reverse load [opposite endian to natural platform endian] of an unsigned 8 byte value
   irlload,     // reverse load [opposite endian to natural platform endian] of a signed 8 byte value
   irsstore,    // reverse store [opposite endian to natural platform endian] of a 2 byte value
   iristore,    // reverse store [opposite endian to natural platform endian] of a 4 byte value
   irlstore,    // reverse store [opposite endian to natural platform endian] of a 8 byte value
   LastJ9Op = irlstore,

#endif
