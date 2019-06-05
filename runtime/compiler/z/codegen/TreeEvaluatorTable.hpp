/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

/*
 * This table is #included in a static table.
 * Only Function Pointers are allowed.
 */

#include "z/codegen/OMRTreeEvaluatorTable.hpp"

   TR::TreeEvaluator::fconstEvaluator,      // TR::dfconst
   TR::TreeEvaluator::dconstEvaluator,      // TR::ddconst
   TR::TreeEvaluator::deconstEvaluator,     // TR::deconst
   TR::TreeEvaluator::floadEvaluator,       // TR::dfload
   TR::TreeEvaluator::dloadEvaluator,       // TR::ddload
   TR::TreeEvaluator::deloadEvaluator,      // TR::deload
   TR::TreeEvaluator::floadEvaluator,       // TR::dfloadi
   TR::TreeEvaluator::dloadEvaluator,       // TR::ddloadi
   TR::TreeEvaluator::deloadEvaluator,      // TR::deloadi
   TR::TreeEvaluator::fstoreEvaluator,      // TR::dfstore
   TR::TreeEvaluator::dstoreEvaluator,      // TR::ddstore
   TR::TreeEvaluator::destoreEvaluator,     // TR::destore
   TR::TreeEvaluator::fstoreEvaluator,      // TR::dfstorei
   TR::TreeEvaluator::dstoreEvaluator,      // TR::ddstorei
   TR::TreeEvaluator::destoreEvaluator,     // TR::destorei
   TR::TreeEvaluator::returnEvaluator,      // TR::dfreturn
   TR::TreeEvaluator::returnEvaluator,      // TR::ddreturn
   TR::TreeEvaluator::returnEvaluator,      // TR::dereturn
   TR::TreeEvaluator::directCallEvaluator,  // TR::dfcall
   TR::TreeEvaluator::directCallEvaluator,  // TR::ddcall
   TR::TreeEvaluator::directCallEvaluator,  // TR::decall
   TR::TreeEvaluator::indirectCallEvaluator,   // TR::idfcall
   TR::TreeEvaluator::indirectCallEvaluator,   // TR::iddcall
   TR::TreeEvaluator::indirectCallEvaluator,   // TR::idecall
   TR::TreeEvaluator::dfaddEvaluator,       // TR::dfadd
   TR::TreeEvaluator::ddaddEvaluator,       // TR::ddadd
   TR::TreeEvaluator::deaddEvaluator,       // TR::deadd
   TR::TreeEvaluator::dfsubEvaluator,       // TR::dfsub
   TR::TreeEvaluator::ddsubEvaluator,       // TR::ddsub
   TR::TreeEvaluator::desubEvaluator,       // TR::desub
   TR::TreeEvaluator::dfmulEvaluator,       // TR::dfmul
   TR::TreeEvaluator::ddmulEvaluator,       // TR::ddmul
   TR::TreeEvaluator::demulEvaluator,       // TR::demul
   TR::TreeEvaluator::dfdivEvaluator,       // TR::dfdiv
   TR::TreeEvaluator::dfdivEvaluator,       // TR::dddiv
   TR::TreeEvaluator::dfdivEvaluator,       // TR::dediv
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfrem
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ddrem
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::derem
   TR::TreeEvaluator::ddnegEvaluator,       // TR::dfneg
   TR::TreeEvaluator::ddnegEvaluator,       // TR::ddneg
   TR::TreeEvaluator::ddnegEvaluator,       // TR::deneg
   TR::TreeEvaluator::iabsEvaluator,        // TR::dfabs
   TR::TreeEvaluator::iabsEvaluator,        // TR::ddabs
   TR::TreeEvaluator::iabsEvaluator,        // TR::deabs
   TR::TreeEvaluator::ddshlEvaluator,       // TR::dfshl
   TR::TreeEvaluator::ddshrEvaluator,       // TR::dfshr
   TR::TreeEvaluator::ddshlEvaluator,       // TR::ddshl
   TR::TreeEvaluator::ddshrEvaluator,       // TR::ddshr
   TR::TreeEvaluator::ddshlEvaluator,       // TR::deshl
   TR::TreeEvaluator::ddshrEvaluator,       // TR::deshr
   TR::TreeEvaluator::ddshrRoundedEvaluator,      // TR::dfshrRounded
   TR::TreeEvaluator::ddshrRoundedEvaluator,      // TR::ddshrRounded
   TR::TreeEvaluator::ddshrRoundedEvaluator,      // TR::deshrRounded
   TR::TreeEvaluator::ddSetNegativeEvaluator,     // TR::dfSetNegative
   TR::TreeEvaluator::ddSetNegativeEvaluator,     // TR::ddSetNegative
   TR::TreeEvaluator::ddSetNegativeEvaluator,     // TR::deSetNegative
   TR::TreeEvaluator::ddModifyPrecisionEvaluator, // TR::dfModifyPrecision
   TR::TreeEvaluator::ddModifyPrecisionEvaluator, // TR::ddModifyPrecision
   TR::TreeEvaluator::ddModifyPrecisionEvaluator, // TR::deModifyPrecision

   TR::TreeEvaluator::unImpOpEvaluator,     // TR::i2df
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::iu2df
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::l2df
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::lu2df
   TR::TreeEvaluator::f2dfEvaluator,        // TR::f2df
   TR::TreeEvaluator::f2dfEvaluator,        // TR::d2df
   TR::TreeEvaluator::dd2dfEvaluator,       // TR::dd2df
   TR::TreeEvaluator::de2ddEvaluator,       // TR::de2df
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::b2df
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::bu2df
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::s2df
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::su2df

   TR::TreeEvaluator::dd2iEvaluator,        // TR::df2i
   TR::TreeEvaluator::dd2iEvaluator,        // TR::df2iu
   TR::TreeEvaluator::dd2lEvaluator,        // TR::df2l
   TR::TreeEvaluator::dd2lEvaluator,        // TR::df2lu
   TR::TreeEvaluator::df2fEvaluator,        // TR::df2f
   TR::TreeEvaluator::df2fEvaluator,        // TR::df2d
   TR::TreeEvaluator::df2ddEvaluator,       // TR::df2dd
   TR::TreeEvaluator::df2deEvaluator,       // TR::df2de
   TR::TreeEvaluator::dd2iEvaluator,        // TR::df2b
   TR::TreeEvaluator::dd2iEvaluator,        // TR::df2bu
   TR::TreeEvaluator::dd2iEvaluator,        // TR::df2s
   TR::TreeEvaluator::dd2iEvaluator,        // TR::df2c

   TR::TreeEvaluator::i2ddEvaluator,        // TR::i2dd
   TR::TreeEvaluator::i2ddEvaluator,        // TR::iu2dd
   TR::TreeEvaluator::l2ddEvaluator,        // TR::l2dd
   TR::TreeEvaluator::lu2ddEvaluator,       // TR::lu2dd
   TR::TreeEvaluator::f2dfEvaluator,        // TR::f2dd
   TR::TreeEvaluator::f2dfEvaluator,        // TR::d2dd
   TR::TreeEvaluator::de2ddEvaluator,       // TR::de2dd
   TR::TreeEvaluator::i2ddEvaluator,        // TR::b2dd
   TR::TreeEvaluator::i2ddEvaluator,        // TR::bu2dd
   TR::TreeEvaluator::i2ddEvaluator,        // TR::s2dd
   TR::TreeEvaluator::i2ddEvaluator,        // TR::su2dd

   TR::TreeEvaluator::dd2iEvaluator,        // TR::dd2i
   TR::TreeEvaluator::dd2iEvaluator,        // TR::dd2iu
   TR::TreeEvaluator::dd2lEvaluator,        // TR::dd2l
   TR::TreeEvaluator::dd2luEvaluator,       // TR::dd2lu
   TR::TreeEvaluator::df2fEvaluator,        // TR::dd2f
   TR::TreeEvaluator::df2fEvaluator,        // TR::dd2d
   TR::TreeEvaluator::dd2deEvaluator,       // TR::dd2de
   TR::TreeEvaluator::dd2iEvaluator,        // TR::dd2b
   TR::TreeEvaluator::dd2iEvaluator,        // TR::dd2bu
   TR::TreeEvaluator::dd2iEvaluator,        // TR::dd2s
   TR::TreeEvaluator::dd2iEvaluator,        // TR::dd2c

   TR::TreeEvaluator::i2ddEvaluator,        // TR::i2de
   TR::TreeEvaluator::i2ddEvaluator,        // TR::iu2de
   TR::TreeEvaluator::l2ddEvaluator,        // TR::l2de
   TR::TreeEvaluator::lu2ddEvaluator,       // TR::lu2de
   TR::TreeEvaluator::f2dfEvaluator,        // TR::f2de
   TR::TreeEvaluator::f2dfEvaluator,        // TR::d2de
   TR::TreeEvaluator::i2ddEvaluator,        // TR::b2de
   TR::TreeEvaluator::i2ddEvaluator,        // TR::bu2de
   TR::TreeEvaluator::i2ddEvaluator,        // TR::s2de
   TR::TreeEvaluator::i2ddEvaluator,        // TR::su2de

   TR::TreeEvaluator::dd2iEvaluator,        // TR::de2i
   TR::TreeEvaluator::dd2iEvaluator,        // TR::de2iu
   TR::TreeEvaluator::dd2lEvaluator,        // TR::de2l
   TR::TreeEvaluator::dd2luEvaluator,       // TR::de2lu
   TR::TreeEvaluator::df2fEvaluator,        // TR::de2f
   TR::TreeEvaluator::df2fEvaluator,        // TR::de2d
   TR::TreeEvaluator::dd2iEvaluator,        // TR::de2b
   TR::TreeEvaluator::dd2iEvaluator,        // TR::de2bu
   TR::TreeEvaluator::dd2iEvaluator,        // TR::de2s
   TR::TreeEvaluator::dd2iEvaluator,        // TR::de2c

   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ifdfcmpeq
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ifdfcmpne
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ifdfcmplt
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ifdfcmpge
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ifdfcmpgt
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ifdfcmple
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ifdfcmpequ
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ifdfcmpneu
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ifdfcmpltu
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ifdfcmpgeu
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ifdfcmpgtu
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ifdfcmpleu

   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfcmpeq
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfcmpne
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfcmplt
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfcmpge
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfcmpgt
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfcmple
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfcmpequ
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfcmpneu
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfcmpltu
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfcmpgeu
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfcmpgtu
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfcmpleu

   TR::TreeEvaluator::ifddcmpeqEvaluator,   // TR::ifddcmpeq
   TR::TreeEvaluator::ifddcmpneEvaluator,   // TR::ifddcmpne
   TR::TreeEvaluator::ifddcmpltEvaluator,   // TR::ifddcmplt
   TR::TreeEvaluator::ifddcmpgeEvaluator,   // TR::ifddcmpge
   TR::TreeEvaluator::ifddcmpgtEvaluator,   // TR::ifddcmpgt
   TR::TreeEvaluator::ifddcmpleEvaluator,   // TR::ifddcmple
   TR::TreeEvaluator::ifddcmpequEvaluator,  // TR::ifddcmpequ
   TR::TreeEvaluator::ifddcmpneuEvaluator,  // TR::ifddcmpneu
   TR::TreeEvaluator::ifddcmpltuEvaluator,  // TR::ifddcmpltu
   TR::TreeEvaluator::ifddcmpgeuEvaluator,  // TR::ifddcmpgeu
   TR::TreeEvaluator::ifddcmpgtuEvaluator,  // TR::ifddcmpgtu
   TR::TreeEvaluator::ifddcmpleuEvaluator,  // TR::ifddcmpleu

   TR::TreeEvaluator::ddcmpeqEvaluator,     // TR::ddcmpeq
   TR::TreeEvaluator::ddcmpneEvaluator,     // TR::ddcmpne
   TR::TreeEvaluator::ddcmpltEvaluator,     // TR::ddcmplt
   TR::TreeEvaluator::ddcmpgeEvaluator,     // TR::ddcmpge
   TR::TreeEvaluator::ddcmpgtEvaluator,     // TR::ddcmpgt
   TR::TreeEvaluator::ddcmpleEvaluator,     // TR::ddcmple
   TR::TreeEvaluator::ddcmpequEvaluator,    // TR::ddcmpequ
   TR::TreeEvaluator::ddcmpneuEvaluator,    // TR::ddcmpneu
   TR::TreeEvaluator::ddcmpltuEvaluator,    // TR::ddcmpltu
   TR::TreeEvaluator::ddcmpgeuEvaluator,    // TR::ddcmpgeu
   TR::TreeEvaluator::ddcmpgtuEvaluator,    // TR::ddcmpgtu
   TR::TreeEvaluator::ddcmpleuEvaluator,    // TR::ddcmpleu

   TR::TreeEvaluator::ifddcmpeqEvaluator,   // TR::ifdecmpeq
   TR::TreeEvaluator::ifddcmpneEvaluator,   // TR::ifdecmpne
   TR::TreeEvaluator::ifddcmpltEvaluator,   // TR::ifdecmplt
   TR::TreeEvaluator::ifddcmpgeEvaluator,   // TR::ifdecmpge
   TR::TreeEvaluator::ifddcmpgtEvaluator,   // TR::ifdecmpgt
   TR::TreeEvaluator::ifddcmpleEvaluator,   // TR::ifdecmple
   TR::TreeEvaluator::ifddcmpequEvaluator,  // TR::ifdecmpequ
   TR::TreeEvaluator::ifddcmpneuEvaluator,  // TR::ifdecmpneu
   TR::TreeEvaluator::ifddcmpltuEvaluator,  // TR::ifdecmpltu
   TR::TreeEvaluator::ifddcmpgeuEvaluator,  // TR::ifdecmpgeu
   TR::TreeEvaluator::ifddcmpgtuEvaluator,  // TR::ifdecmpgtu
   TR::TreeEvaluator::ifddcmpleuEvaluator,  // TR::ifdecmpleu

   TR::TreeEvaluator::ddcmpeqEvaluator,     // TR::decmpeq
   TR::TreeEvaluator::ddcmpneEvaluator,     // TR::decmpne
   TR::TreeEvaluator::ddcmpltEvaluator,     // TR::decmplt
   TR::TreeEvaluator::ddcmpgeEvaluator,     // TR::decmpge
   TR::TreeEvaluator::ddcmpgtEvaluator,     // TR::decmpgt
   TR::TreeEvaluator::ddcmpleEvaluator,     // TR::decmple
   TR::TreeEvaluator::ddcmpequEvaluator,    // TR::decmpequ
   TR::TreeEvaluator::ddcmpneuEvaluator,    // TR::decmpneu
   TR::TreeEvaluator::ddcmpltuEvaluator,    // TR::decmpltu
   TR::TreeEvaluator::ddcmpgeuEvaluator,    // TR::decmpgeu
   TR::TreeEvaluator::ddcmpgtuEvaluator,    // TR::decmpgtu
   TR::TreeEvaluator::ddcmpleuEvaluator,    // TR::decmpleu

   TR::TreeEvaluator::fRegLoadEvaluator,    // TR::dfRegLoad
   TR::TreeEvaluator::dRegLoadEvaluator,    // TR::ddRegLoad
   TR::TreeEvaluator::deRegLoadEvaluator,   // TR::deRegLoad
   TR::TreeEvaluator::fRegStoreEvaluator,   // TR::dfRegStore
   TR::TreeEvaluator::dRegStoreEvaluator,   // TR::ddRegStore
   TR::TreeEvaluator::deRegStoreEvaluator,  // TR::deRegStore

   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfternary
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ddternary
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::deternary

   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfexp
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ddexp
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::deexp
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfnint
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ddnint
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::denint
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfsqrt
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ddsqrt
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::desqrt

   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfcos
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ddcos
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::decos
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfsin
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ddsin
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::desin
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dftan
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ddtan
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::detan

   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfcosh
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ddcosh
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::decosh
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfsinh
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ddsinh
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::desinh
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dftanh
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ddtanh
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::detanh

   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfacos
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ddacos
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::deacos
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfasin
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ddasin
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::deasin
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfatan
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ddatan
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::deatan

   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfatan2
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ddatan2
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::deatan2
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dflog
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ddlog
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::delog
   TR::TreeEvaluator::dffloorEvaluator,     // TR::dffloor
   TR::TreeEvaluator::ddfloorEvaluator,     // TR::ddfloor
   TR::TreeEvaluator::defloorEvaluator,     // TR::defloor
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfceil
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ddceil
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::deceil
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfmax
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ddmax
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::demax
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfmin
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ddmin
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::demin

   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dfInsExp
   TR::TreeEvaluator::ddInsExpEvaluator,    // TR::ddInsExp
   TR::TreeEvaluator::ddInsExpEvaluator,    // TR::deInsExp

   TR::TreeEvaluator::ddcleanEvaluator,     // TR::ddclean
   TR::TreeEvaluator::decleanEvaluator,     // TR::declean

   TR::TreeEvaluator::pdloadEvaluator,      // TR::zdload
   TR::TreeEvaluator::pdloadEvaluator,      // TR::zdloadi
   TR::TreeEvaluator::pdstoreEvaluator,     // TR::zdstore
   TR::TreeEvaluator::pdstoreEvaluator,     // TR::zdstorei

   TR::TreeEvaluator::pd2zdEvaluator,       // TR::pd2zd
   TR::TreeEvaluator::zd2pdEvaluator,       // TR::zd2pd

   TR::TreeEvaluator::pdloadEvaluator,      // TR::zdsleLoad
   TR::TreeEvaluator::pdloadEvaluator,      // TR::zdslsLoad
   TR::TreeEvaluator::pdloadEvaluator,      // TR::zdstsLoad

   TR::TreeEvaluator::pdloadEvaluator,      // TR::zdsleLoadi
   TR::TreeEvaluator::pdloadEvaluator,      // TR::zdslsLoadi
   TR::TreeEvaluator::pdloadEvaluator,      // TR::zdstsLoadi

   TR::TreeEvaluator::pdstoreEvaluator,     // TR::zdsleStore
   TR::TreeEvaluator::pdstoreEvaluator,     // TR::zdslsStore
   TR::TreeEvaluator::pdstoreEvaluator,     // TR::zdstsStore

   TR::TreeEvaluator::pdstoreEvaluator,     // TR::zdsleStorei
   TR::TreeEvaluator::pdstoreEvaluator,     // TR::zdslsStorei
   TR::TreeEvaluator::pdstoreEvaluator,     // TR::zdstsStorei

   TR::TreeEvaluator::zdsle2zdEvaluator,    // TR::zd2zdsle
   TR::TreeEvaluator::zd2zdslsEvaluator,    // TR::zd2zdsls
   TR::TreeEvaluator::zd2zdslsEvaluator,    // TR::zd2zdsts

   TR::TreeEvaluator::unImpOpEvaluator,     // TR::zdsle2pd
   TR::TreeEvaluator::zdsls2pdEvaluator,    // TR::zdsls2pd
   TR::TreeEvaluator::zdsls2pdEvaluator,    // TR::zdsts2pd

   TR::TreeEvaluator::zdsle2zdEvaluator,    // TR::zdsle2zd
   TR::TreeEvaluator::zdsls2zdEvaluator,    // TR::zdsls2zd
   TR::TreeEvaluator::zdsls2zdEvaluator,    // TR::zdsts2zd

   TR::TreeEvaluator::pd2zdslsEvaluator,    // TR::pd2zdsls
   TR::TreeEvaluator::pd2zdslsEvaluator,    // TR::pd2zdslsSetSign
   TR::TreeEvaluator::pd2zdslsEvaluator,    // TR::pd2zdsts
   TR::TreeEvaluator::pd2zdslsEvaluator,    // TR::pd2zdstsSetSign

   TR::TreeEvaluator::unImpOpEvaluator,     // TR::zd2df
   TR::TreeEvaluator::df2zdEvaluator,       // TR::df2zd
   TR::TreeEvaluator::zd2ddEvaluator,       // TR::zd2dd
   TR::TreeEvaluator::df2zdEvaluator,       // TR::dd2zd
   TR::TreeEvaluator::zd2ddEvaluator,       // TR::zd2de
   TR::TreeEvaluator::df2zdEvaluator,       // TR::de2zd

   TR::TreeEvaluator::unImpOpEvaluator,     // TR::zd2dfAbs
   TR::TreeEvaluator::zd2ddEvaluator,       // TR::zd2ddAbs
   TR::TreeEvaluator::zd2ddEvaluator,       // TR::zd2deAbs

   TR::TreeEvaluator::df2zdEvaluator,       // TR::df2zdSetSign
   TR::TreeEvaluator::df2zdEvaluator,       // TR::dd2zdSetSign
   TR::TreeEvaluator::df2zdEvaluator,       // TR::de2zdSetSign

   TR::TreeEvaluator::df2zdEvaluator,       // TR::df2zdClean
   TR::TreeEvaluator::df2zdEvaluator,       // TR::dd2zdClean
   TR::TreeEvaluator::df2zdEvaluator,       // TR::de2zdClean

   TR::TreeEvaluator::pdloadEvaluator,      // TR::udLoad
   TR::TreeEvaluator::pdloadEvaluator,      // TR::udslLoad
   TR::TreeEvaluator::pdloadEvaluator,      // TR::udstLoad

   TR::TreeEvaluator::pdloadEvaluator,      // TR::udLoadi
   TR::TreeEvaluator::pdloadEvaluator,      // TR::udslLoadi
   TR::TreeEvaluator::pdloadEvaluator,      // TR::udstLoadi

   TR::TreeEvaluator::pdstoreEvaluator,     // TR::udStore
   TR::TreeEvaluator::pdstoreEvaluator,     // TR::udslStore
   TR::TreeEvaluator::pdstoreEvaluator,     // TR::udstStore

   TR::TreeEvaluator::pdstoreEvaluator,     // TR::udStorei
   TR::TreeEvaluator::pdstoreEvaluator,     // TR::udslStorei
   TR::TreeEvaluator::pdstoreEvaluator,     // TR::udstStorei

   TR::TreeEvaluator::pd2udEvaluator,       // TR::pd2ud
   TR::TreeEvaluator::pd2udslEvaluator,     // TR::pd2udsl
   TR::TreeEvaluator::pd2udslEvaluator,     // TR::pd2udst

   TR::TreeEvaluator::unImpOpEvaluator,     // TR::udsl2ud
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::udst2ud

   TR::TreeEvaluator::ud2pdEvaluator,       // TR::ud2pd
   TR::TreeEvaluator::udsl2pdEvaluator,     // TR::udsl2pd
   TR::TreeEvaluator::udsl2pdEvaluator,     // TR::udst2pd

   TR::TreeEvaluator::pdloadEvaluator,      // TR::pdload
   TR::TreeEvaluator::pdloadEvaluator,      // TR::pdloadi
   TR::TreeEvaluator::pdstoreEvaluator,     // TR::pdstore
   TR::TreeEvaluator::pdstoreEvaluator,     // TR::pdstorei
   TR::TreeEvaluator::pdaddEvaluator,       // TR::pdadd
   TR::TreeEvaluator::pdsubEvaluator,       // TR::pdsub
   TR::TreeEvaluator::pdmulEvaluator,       // TR::pdmul
   TR::TreeEvaluator::pddivremEvaluator,    // TR::pddiv
   TR::TreeEvaluator::pddivremEvaluator,    // TR::pdrem
   TR::TreeEvaluator::pdnegEvaluator,       // TR::pdneg
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::pdabs
   TR::TreeEvaluator::pdshrEvaluator,       // TR::pdshr
   TR::TreeEvaluator::pdshlEvaluator,       // TR::pdshl

   TR::TreeEvaluator::unImpOpEvaluator,     // TR::pdshrSetSign
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::pdshlSetSign
   TR::TreeEvaluator::pdshlEvaluator,       // TR::pdshlOverflow
   TR::TreeEvaluator::pdchkEvaluator,       // TR::pdchk

   TR::TreeEvaluator::pd2iEvaluator,        // TR::pd2i
   TR::TreeEvaluator::pd2iEvaluator,        // TR::pd2iOverflow
   TR::TreeEvaluator::pd2iEvaluator,        // TR::pd2iu
   TR::TreeEvaluator::i2pdEvaluator,        // TR::i2pd
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::iu2pd
   TR::TreeEvaluator::pd2lEvaluator,        // TR::pd2l
   TR::TreeEvaluator::pd2lEvaluator,        // TR::pd2lOverflow
   TR::TreeEvaluator::pd2lEvaluator,        // TR::pd2lu
   TR::TreeEvaluator::l2pdEvaluator,        // TR::l2pd
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::lu2pd
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::pd2f
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::pd2d
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::f2pd
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::d2pd

   TR::TreeEvaluator::pdcmpeqEvaluator,     // TR::pdcmpeq
   TR::TreeEvaluator::pdcmpneEvaluator,     // TR::pdcmpne
   TR::TreeEvaluator::pdcmpltEvaluator,     // TR::pdcmplt
   TR::TreeEvaluator::pdcmpgeEvaluator,     // TR::pdcmpge
   TR::TreeEvaluator::pdcmpgtEvaluator,     // TR::pdcmpgt
   TR::TreeEvaluator::pdcmpleEvaluator,     // TR::pdcmple

   TR::TreeEvaluator::unImpOpEvaluator,     // TR::pdclean

   TR::TreeEvaluator::pdclearEvaluator,     // TR::pdclear
   TR::TreeEvaluator::pdclearEvaluator,     // TR::pdclearSetSign

   TR::TreeEvaluator::pdSetSignEvaluator,    // TR::pdSetSign

   TR::TreeEvaluator::pdModifyPrecisionEvaluator,       // TR::pdModifyPrecision

   TR::TreeEvaluator::countDigitsEvaluator, // TR::countDigits

   TR::TreeEvaluator::unImpOpEvaluator,     // TR::pd2df
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::pd2dfAbs
   TR::TreeEvaluator::df2pdEvaluator,       // TR::df2pd
   TR::TreeEvaluator::df2pdEvaluator,       // TR::df2pdSetSign
   TR::TreeEvaluator::df2pdEvaluator,       // TR::df2pdClean
   TR::TreeEvaluator::pd2ddEvaluator,       // TR::pd2dd
   TR::TreeEvaluator::pd2ddEvaluator,       // TR::pd2ddAbs
   TR::TreeEvaluator::df2pdEvaluator,       // TR::dd2pd
   TR::TreeEvaluator::df2pdEvaluator,       // TR::dd2pdSetSign
   TR::TreeEvaluator::df2pdEvaluator,       // TR::dd2pdClean
   TR::TreeEvaluator::pd2ddEvaluator,       // TR::pd2de
   TR::TreeEvaluator::pd2ddEvaluator,       // TR::pd2deAbs
   TR::TreeEvaluator::df2pdEvaluator,       // TR::de2pd
   TR::TreeEvaluator::df2pdEvaluator,       // TR::de2pdSetSign
   TR::TreeEvaluator::df2pdEvaluator,       // TR::de2pdClean
   TR::TreeEvaluator::BCDCHKEvaluator,      // TR::BCDCHK
   TR::TreeEvaluator::rsloadEvaluator,      // TR::ircload
   TR::TreeEvaluator::rsloadEvaluator,      // TR::irsload
   TR::TreeEvaluator::riloadEvaluator,      // TR::iruiload
   TR::TreeEvaluator::riloadEvaluator,      // TR::iriload
   TR::TreeEvaluator::rlloadEvaluator,      // TR::irulload
   TR::TreeEvaluator::rlloadEvaluator,      // TR::irlload
   TR::TreeEvaluator::rsstoreEvaluator,     // TR::irsstore
   TR::TreeEvaluator::ristoreEvaluator,     // TR::iristore
   TR::TreeEvaluator::rlstoreEvaluator,     // TR::irlstore
