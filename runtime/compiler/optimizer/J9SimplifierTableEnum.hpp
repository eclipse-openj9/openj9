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

#ifndef J9_SIMPLIFIERTABLE_ENUM_INCL
#define J9_SIMPLIFIERTABLE_ENUM_INCL

#include "optimizer/OMRSimplifierTableEnum.hpp"

   dftSimplifier,               // TR::dfconst
   dftSimplifier,               // TR::ddconst
   dftSimplifier,               // TR::deconst
   dftSimplifier,               // TR::dfload
   dftSimplifier,               // TR::ddload
   dftSimplifier,               // TR::deload
   dftSimplifier,               // TR::dfloadi
   dftSimplifier,               // TR::ddloadi
   dftSimplifier,               // TR::deloadi
   dftSimplifier,               // TR::dfstore
   dftSimplifier,               // TR::ddstore
   dftSimplifier,               // TR::destore
   dftSimplifier,               // TR::dfstorei
   dftSimplifier,               // TR::ddstorei
   dftSimplifier,               // TR::destorei
   dftSimplifier,               // TR::dfreturn
   dftSimplifier,               // TR::ddreturn
   dftSimplifier,               // TR::dereturn
   dftSimplifier,               // TR::dfcall
   dftSimplifier,               // TR::ddcall
   dftSimplifier,               // TR::decall
   dftSimplifier,               // TR::idfcall
   dftSimplifier,               // TR::iddcall
   dftSimplifier,               // TR::idecall
   dfpAddSimplifier,            // TR::dfadd
   dfpAddSimplifier,            // TR::ddadd
   dfpAddSimplifier,            // TR::deadd
   dftSimplifier,               // TR::dfsub
   dftSimplifier,               // TR::ddsub
   dftSimplifier,               // TR::desub
   dfpMulSimplifier,            // TR::dfmul
   dfpMulSimplifier,            // TR::ddmul
   dfpMulSimplifier,            // TR::demul
   dfpDivSimplifier,            // TR::dfdiv
   dfpDivSimplifier,            // TR::dddiv
   dfpDivSimplifier,            // TR::dediv
   dftSimplifier,               // TR::dfrem
   dftSimplifier,               // TR::ddrem
   dftSimplifier,               // TR::derem
   dftSimplifier,               // TR::dfneg
   dftSimplifier,               // TR::ddneg
   dftSimplifier,               // TR::deneg
   dfpSetSignSimplifier,        // TR::dfabs
   dfpSetSignSimplifier,        // TR::ddabs
   dfpSetSignSimplifier,        // TR::deabs
   dfpShiftLeftSimplifier,      // TR::dfshl
   dfpShiftRightSimplifier,     // TR::dfshr
   dfpShiftLeftSimplifier,      // TR::ddshl
   dfpShiftRightSimplifier,     // TR::ddshr
   dfpShiftLeftSimplifier,      // TR::deshl
   dfpShiftRightSimplifier,     // TR::deshr
   dfpShiftRightSimplifier,     // TR::dfshrRounded
   dfpShiftRightSimplifier,     // TR::ddshrRounded
   dfpShiftRightSimplifier,     // TR::deshrRounded
   dfpSetSignSimplifier,        // TR::dfSetNegative
   dfpSetSignSimplifier,        // TR::ddSetNegative
   dfpSetSignSimplifier,        // TR::deSetNegative
   dfpModifyPrecisionSimplifier, // TR::dfModifyPrecision
   dfpModifyPrecisionSimplifier, // TR::ddModifyPrecision
   dfpModifyPrecisionSimplifier, // TR::deModifyPrecision

   int2dfpSimplifier,           // TR::i2df
   int2dfpSimplifier,           // TR::iu2df
   int2dfpSimplifier,           // TR::l2df
   int2dfpSimplifier,           // TR::lu2df
   dftSimplifier,               // TR::f2df
   dftSimplifier,               // TR::d2df
   dfp2dfpSimplifier,           // TR::dd2df
   dfp2dfpSimplifier,           // TR::de2df
   dftSimplifier,               // TR::b2df
   dftSimplifier,               // TR::bu2df
   dftSimplifier,               // TR::s2df
   dftSimplifier,               // TR::su2df

   dftSimplifier,               // TR::df2i
   dftSimplifier,               // TR::df2iu
   dftSimplifier,               // TR::df2l
   dftSimplifier,               // TR::df2lu
   dftSimplifier,               // TR::df2f
   dftSimplifier,               // TR::df2d
   dfp2dfpSimplifier,           // TR::df2dd
   dfp2dfpSimplifier,           // TR::df2de
   dftSimplifier,               // TR::df2b
   dftSimplifier,               // TR::df2bu
   dftSimplifier,               // TR::df2s
   dftSimplifier,               // TR::df2c

   int2dfpSimplifier,           // TR::i2dd
   int2dfpSimplifier,           // TR::iu2dd
   int2dfpSimplifier,           // TR::l2dd
   int2dfpSimplifier,           // TR::lu2dd
   dftSimplifier,               // TR::f2dd
   dftSimplifier,               // TR::d2dd
   dfp2dfpSimplifier,           // TR::de2dd
   dftSimplifier,               // TR::b2dd
   dftSimplifier,               // TR::bu2dd
   dftSimplifier,               // TR::s2dd
   dftSimplifier,               // TR::su2dd

   dfp2intSimplifier,           // TR::dd2i
   dfp2intSimplifier,           // TR::dd2iu
   dfp2intSimplifier,           // TR::dd2l
   dfp2intSimplifier,           // TR::dd2lu
   dftSimplifier,               // TR::dd2f
   dftSimplifier,               // TR::dd2d
   dfp2dfpSimplifier,           // TR::dd2de
   dftSimplifier,               // TR::dd2b
   dftSimplifier,               // TR::dd2bu
   dftSimplifier,               // TR::dd2s
   dftSimplifier,               // TR::dd2c

   int2dfpSimplifier,           // TR::i2de
   int2dfpSimplifier,           // TR::iu2de
   int2dfpSimplifier,           // TR::l2de
   int2dfpSimplifier,           // TR::lu2de
   dftSimplifier,               // TR::f2de
   dftSimplifier,               // TR::d2de
   dftSimplifier,               // TR::b2de
   dftSimplifier,               // TR::bu2de
   dftSimplifier,               // TR::s2de
   dftSimplifier,               // TR::su2de

   dftSimplifier,               // TR::de2i
   dftSimplifier,               // TR::de2iu
   dftSimplifier,               // TR::de2l
   dftSimplifier,               // TR::de2lu
   dftSimplifier,               // TR::de2f
   dftSimplifier,               // TR::de2d
   dftSimplifier,               // TR::de2b
   dftSimplifier,               // TR::de2bu
   dftSimplifier,               // TR::de2s
   dftSimplifier,               // TR::de2c

   ifDFPCompareSimplifier,      // TR::ifdfcmpeq
   ifDFPCompareSimplifier,      // TR::ifdfcmpne
   ifDFPCompareSimplifier,      // TR::ifdfcmplt
   ifDFPCompareSimplifier,      // TR::ifdfcmpge
   ifDFPCompareSimplifier,      // TR::ifdfcmpgt
   ifDFPCompareSimplifier,      // TR::ifdfcmple
   dftSimplifier,               // TR::ifdfcmpequ
   dftSimplifier,               // TR::ifdfcmpneu
   dftSimplifier,               // TR::ifdfcmpltu
   dftSimplifier,               // TR::ifdfcmpgeu
   dftSimplifier,               // TR::ifdfcmpgtu
   dftSimplifier,               // TR::ifdfcmpleu

   dftSimplifier,               // TR::dfcmpeq
   dftSimplifier,               // TR::dfcmpne
   dftSimplifier,               // TR::dfcmplt
   dftSimplifier,               // TR::dfcmpge
   dftSimplifier,               // TR::dfcmpgt
   dftSimplifier,               // TR::dfcmple
   dftSimplifier,               // TR::dfcmpequ
   dftSimplifier,               // TR::dfcmpneu
   dftSimplifier,               // TR::dfcmpltu
   dftSimplifier,               // TR::dfcmpgeu
   dftSimplifier,               // TR::dfcmpgtu
   dftSimplifier,               // TR::dfcmpleu

   ifDFPCompareSimplifier,      // TR::ifddcmpeq
   ifDFPCompareSimplifier,      // TR::ifddcmpne
   ifDFPCompareSimplifier,      // TR::ifddcmplt
   ifDFPCompareSimplifier,      // TR::ifddcmpge
   ifDFPCompareSimplifier,      // TR::ifddcmpgt
   ifDFPCompareSimplifier,      // TR::ifddcmple
   dftSimplifier,               // TR::ifddcmpequ
   dftSimplifier,               // TR::ifddcmpneu
   dftSimplifier,               // TR::ifddcmpltu
   dftSimplifier,               // TR::ifddcmpgeu
   dftSimplifier,               // TR::ifddcmpgtu
   dftSimplifier,               // TR::ifddcmpleu

   dftSimplifier,               // TR::ddcmpeq
   dftSimplifier,               // TR::ddcmpne
   dftSimplifier,               // TR::ddcmplt
   dftSimplifier,               // TR::ddcmpge
   dftSimplifier,               // TR::ddcmpgt
   dftSimplifier,               // TR::ddcmple
   dftSimplifier,               // TR::ddcmpequ
   dftSimplifier,               // TR::ddcmpneu
   dftSimplifier,               // TR::ddcmpltu
   dftSimplifier,               // TR::ddcmpgeu
   dftSimplifier,               // TR::ddcmpgtu
   dftSimplifier,               // TR::ddcmpleu

   ifDFPCompareSimplifier,      // TR::ifdecmpeq
   ifDFPCompareSimplifier,      // TR::ifdecmpne
   ifDFPCompareSimplifier,      // TR::ifdecmplt
   ifDFPCompareSimplifier,      // TR::ifdecmpge
   ifDFPCompareSimplifier,      // TR::ifdecmpgt
   ifDFPCompareSimplifier,      // TR::ifdecmple
   dftSimplifier,               // TR::ifdecmpequ
   dftSimplifier,               // TR::ifdecmpneu
   dftSimplifier,               // TR::ifdecmpltu
   dftSimplifier,               // TR::ifdecmpgeu
   dftSimplifier,               // TR::ifdecmpgtu
   dftSimplifier,               // TR::ifdecmpleu

   dftSimplifier,               // TR::decmpeq
   dftSimplifier,               // TR::decmpne
   dftSimplifier,               // TR::decmplt
   dftSimplifier,               // TR::decmpge
   dftSimplifier,               // TR::decmpgt
   dftSimplifier,               // TR::decmple
   dftSimplifier,               // TR::decmpequ
   dftSimplifier,               // TR::decmpneu
   dftSimplifier,               // TR::decmpltu
   dftSimplifier,               // TR::decmpgeu
   dftSimplifier,               // TR::decmpgtu
   dftSimplifier,               // TR::decmpleu

   dftSimplifier,               // TR::dfRegLoad
   dftSimplifier,               // TR::ddRegLoad
   dftSimplifier,               // TR::deRegLoad
   dftSimplifier,               // TR::dfRegStore
   dftSimplifier,               // TR::ddRegStore
   dftSimplifier,               // TR::deRegStore

   dftSimplifier,               // TR::dfternary
   dftSimplifier,               // TR::ddternary
   dftSimplifier,               // TR::deternary

   dftSimplifier,               // TR::dfexp
   dftSimplifier,               // TR::ddexp
   dftSimplifier,               // TR::deexp
   dftSimplifier,               // TR::dfnint
   dftSimplifier,               // TR::ddnint
   dftSimplifier,               // TR::denint
   dftSimplifier,               // TR::dfsqrt
   dftSimplifier,               // TR::ddsqrt
   dftSimplifier,               // TR::desqrt

   dftSimplifier,               // TR::dfcos
   dftSimplifier,               // TR::ddcos
   dftSimplifier,               // TR::decos
   dftSimplifier,               // TR::dfsin
   dftSimplifier,               // TR::ddsin
   dftSimplifier,               // TR::desin
   dftSimplifier,               // TR::dftan
   dftSimplifier,               // TR::ddtan
   dftSimplifier,               // TR::detan

   dftSimplifier,               // TR::dfcosh
   dftSimplifier,               // TR::ddcosh
   dftSimplifier,               // TR::decosh
   dftSimplifier,               // TR::dfsinh
   dftSimplifier,               // TR::ddsinh
   dftSimplifier,               // TR::desinh
   dftSimplifier,               // TR::dftanh
   dftSimplifier,               // TR::ddtanh
   dftSimplifier,               // TR::detanh

   dftSimplifier,               // TR::dfacos
   dftSimplifier,               // TR::ddacos
   dftSimplifier,               // TR::deacos
   dftSimplifier,               // TR::dfasin
   dftSimplifier,               // TR::ddasin
   dftSimplifier,               // TR::deasin
   dftSimplifier,               // TR::dfatan
   dftSimplifier,               // TR::ddatan
   dftSimplifier,               // TR::deatan

   dftSimplifier,               // TR::dfatan2
   dftSimplifier,               // TR::ddatan2
   dftSimplifier,               // TR::deatan2
   dftSimplifier,               // TR::dflog
   dftSimplifier,               // TR::ddlog
   dftSimplifier,               // TR::delog
   dfpFloorSimplifier,          // TR::dffloor
   dfpFloorSimplifier,          // TR::ddfloor
   dfpFloorSimplifier,          // TR::defloor
   dftSimplifier,               // TR::dfceil
   dftSimplifier,               // TR::ddceil
   dftSimplifier,               // TR::deceil
   dftSimplifier,               // TR::dfmax
   dftSimplifier,               // TR::ddmax
   dftSimplifier,               // TR::demax
   dftSimplifier,               // TR::dfmin
   dftSimplifier,               // TR::ddmin
   dftSimplifier,               // TR::demin

   dftSimplifier,               // TR::dfInsExp
   dftSimplifier,               // TR::ddInsExp
   dftSimplifier,               // TR::deInsExp

   dftSimplifier,               // TR::ddclean
   dftSimplifier,               // TR::declean

   zdloadSimplifier,            // TR::zdload
   zdloadSimplifier,            // TR::zdloadi
   pdstoreSimplifier,           // TR::zdstore
   pdstoreSimplifier,           // TR::zdstorei

   pd2zdSimplifier,             // TR::pd2zd
   zd2pdSimplifier,             // TR::zd2pd

   dftSimplifier,               // TR::zdsleLoad
   dftSimplifier,               // TR::zdslsLoad
   dftSimplifier,               // TR::zdstsLoad

   dftSimplifier,               // TR::zdsleLoadi
   dftSimplifier,               // TR::zdslsLoadi
   dftSimplifier,               // TR::zdstsLoadi

   zdsleStoreSimplifier,        // TR::zdsleStore
   dftSimplifier,               // TR::zdslsStore
   zdstsStoreSimplifier,        // TR::zdstsStore

   zdsleStoreSimplifier,        // TR::zdsleStorei
   dftSimplifier,               // TR::zdslsStorei
   zdstsStoreSimplifier,        // TR::zdstsStorei

   zd2zdsleSimplifier,          // TR::zd2zdsle
   zd2zdslsSimplifier,          // TR::zd2zdsls
   zd2zdslsSimplifier,          // TR::zd2zdsts

   zdsle2pdSimplifier,          // TR::zdsle2pd
   zdsls2pdSimplifier,          // TR::zdsls2pd
   zdsls2pdSimplifier,          // TR::zdsts2pd

   zdsle2zdSimplifier,          // TR::zdsle2zd
   zdsls2zdSimplifier,          // TR::zdsls2zd
   zdsls2zdSimplifier,          // TR::zdsts2zd

   pd2zdslsSimplifier,          // TR::pd2zdsls
   pd2zdslsSetSignSimplifier,   // TR::pd2zdslsSetSign
   pd2zdslsSimplifier,          // TR::pd2zdsts
   pd2zdslsSetSignSimplifier,   // TR::pd2zdstsSetSign

   zd2dfSimplifier,             // TR::zd2df
   df2zdSimplifier,             // TR::df2zd
   zd2dfSimplifier,             // TR::zd2dd
   df2zdSimplifier,             // TR::dd2zd
   zd2dfSimplifier,             // TR::zd2de
   df2zdSimplifier,             // TR::de2zd

   zd2dfSimplifier,             // TR::zd2dfAbs
   zd2dfSimplifier,             // TR::zd2ddAbs
   zd2dfSimplifier,             // TR::zd2deAbs

   df2zdSetSignSimplifier,      // TR::df2zdSetSign
   df2zdSetSignSimplifier,      // TR::dd2zdSetSign
   df2zdSetSignSimplifier,      // TR::de2zdSetSign

   dftSimplifier,               // TR::df2zdClean
   dftSimplifier,               // TR::dd2zdClean
   dftSimplifier,               // TR::de2zdClean

   dftSimplifier,               // TR::udLoad
   dftSimplifier,               // TR::udslLoad
   dftSimplifier,               // TR::udstLoad

   dftSimplifier,               // TR::udLoadi
   dftSimplifier,               // TR::udslLoadi
   dftSimplifier,               // TR::udstLoadi

   pdstoreSimplifier,           // TR::udStore
   dftSimplifier,               // TR::udslStore
   dftSimplifier,               // TR::udstStore

   pdstoreSimplifier,           // TR::udStorei
   dftSimplifier,               // TR::udslStorei
   dftSimplifier,               // TR::udstStorei

   pd2udSimplifier,             // TR::pd2ud
   pd2udslSimplifier,           // TR::pd2udsl
   pd2udslSimplifier,           // TR::pd2udst

   udsl2udSimplifier,           // TR::udsl2ud
   udsl2udSimplifier,           // TR::udst2ud

   ud2pdSimplifier,             // TR::ud2pd
   udsx2pdSimplifier,           // TR::udsl2pd
   udsx2pdSimplifier,           // TR::udst2pd

   pdloadSimplifier,            // TR::pdload
   pdloadSimplifier,            // TR::pdloadi
   pdstoreSimplifier,           // TR::pdstore
   pdstoreSimplifier,           // TR::pdstorei
   pdaddSimplifier,             // TR::pdadd
   pdsubSimplifier,             // TR::pdsub
   pdmulSimplifier,             // TR::pdmul
   pddivSimplifier,             // TR::pddiv
   dftSimplifier,               // TR::pdrem
   pdnegSimplifier,             // TR::pdneg
   dftSimplifier,               // TR::pdabs
   pdshrSimplifier,             // TR::pdshr
   pdshlSimplifier,             // TR::pdshl

   pdshrSetSignSimplifier,      // TR::pdshrSetSign
   pdshlSetSignSimplifier,      // TR::pdshlSetSign
   dftSimplifier,               // TR::pdshlOverflow
   dftSimplifier,               // TR::pdchk
   pd2iSimplifier,              // TR::pd2i
   dftSimplifier,               // TR::pd2iOverflow
   dftSimplifier,               // TR::pd2iu
   i2pdSimplifier,              // TR::i2pd
   dftSimplifier,               // TR::iu2pd
   pd2lSimplifier,              // TR::pd2l
   dftSimplifier,               // TR::pd2lOverflow
   pd2lSimplifier,              // TR::pd2lu
   i2pdSimplifier,              // TR::l2pd
   dftSimplifier,               // TR::lu2pd
   pd2fSimplifier,              // TR::pd2f
   pd2fSimplifier,              // TR::pd2d
   f2pdSimplifier,              // TR::f2pd
   f2pdSimplifier,              // TR::d2pd

   dftSimplifier,               // TR::pdcmpeq
   dftSimplifier,               // TR::pdcmpne
   dftSimplifier,               // TR::pdcmplt
   dftSimplifier,               // TR::pdcmpge
   dftSimplifier,               // TR::pdcmpgt
   dftSimplifier,               // TR::pdcmple

   pdcleanSimplifier,           // TR::pdclean
   pdclearSimplifier,           // TR::pdclear
   pdclearSetSignSimplifier,    // TR::pdclearSetSign

   pdSetSignSimplifier,         // TR::pdSetSign

   pdshlSimplifier,             // TR::pdModifyPrecision

   dftSimplifier,               // TR::countDigits

   pd2dfSimplifier,             // TR::pd2df
   pd2dfSimplifier,             // TR::pd2dfAbs
   df2pdSimplifier,             // TR::df2pd
   df2pdSetSignSimplifier,      // TR::df2pdSetSign
   df2pdSimplifier,             // TR::df2pdClean
   pd2dfSimplifier,             // TR::pd2dd
   pd2dfSimplifier,             // TR::pd2ddAbs
   df2pdSimplifier,             // TR::dd2pd
   df2pdSetSignSimplifier,      // TR::dd2pdSetSign
   df2pdSimplifier,             // TR::dd2pdClean
   pd2dfSimplifier,             // TR::pd2de
   pd2dfSimplifier,             // TR::pd2deAbs
   df2pdSimplifier,             // TR::de2pd
   df2pdSetSignSimplifier,      // TR::de2pdSetSign
   df2pdSimplifier,             // TR::de2pdClean
   dftSimplifier,               // TR::BCDCHK
   dftSimplifier,               // TR::ircload
   dftSimplifier,               // TR::irsload
   dftSimplifier,               // TR::iruiload
   dftSimplifier,               // TR::iriload
   dftSimplifier,               // TR::irulload
   dftSimplifier,               // TR::irlload
   dftSimplifier,               // TR::irsstore
   dftSimplifier,               // TR::iristore
   dftSimplifier,               // TR::irlstore
#endif
