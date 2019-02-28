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

#include "x/amd64/codegen/OMRTreeEvaluatorTable.hpp"

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfconst
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddconst
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deconst
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfload
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddload
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deload
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfloadi
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddloadi
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deloadi
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfstore
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddstore
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::destore
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfstorei
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddstorei
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::destorei
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfreturn
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddreturn
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dereturn
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfcall
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddcall
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::decall
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::idfcall
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::iddcall
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::idecall
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfadd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddadd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deadd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfsub
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddsub
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::desub
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfmul
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddmul
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::demul
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfdiv
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dddiv
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dediv
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfrem
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddrem
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::derem
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfneg
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddneg
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deneg
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfabs
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddabs
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deabs
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfshl
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfshr
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddshl
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddshr
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deshl
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deshr
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfshrRounded
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddshrRounded
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deshrRounded
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfSetNegative
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddSetNegative
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deSetNegative
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfModifyPrecision
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddModifyPrecision
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deModifyPrecision

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::i2df
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::iu2df
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::l2df
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::lu2df
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::f2df
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::d2df
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dd2df
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::de2df
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::b2df
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::bu2df
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::s2df
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::su2df

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::df2i
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::df2iu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::df2l
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::df2lu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::df2f
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::df2d
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::df2dd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::df2de
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::df2b
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::df2bu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::df2s
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::df2c

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::i2dd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::iu2dd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::l2dd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::lu2dd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::f2dd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::d2dd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::de2dd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::b2dd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::bu2dd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::s2dd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::su2dd

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dd2i
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dd2iu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dd2l
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dd2lu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dd2f
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dd2d
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dd2de
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dd2b
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dd2bu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dd2s
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dd2c

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::i2de
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::iu2de
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::l2de
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::lu2de
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::f2de
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::d2de
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::b2de
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::bu2de
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::s2de
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::su2de

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::de2i
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::de2iu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::de2l
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::de2lu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::de2f
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::de2d
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::de2b
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::de2bu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::de2s
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::de2c

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdfcmpeq
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdfcmpne
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdfcmplt
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdfcmpge
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdfcmpgt
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdfcmple
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdfcmpequ
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdfcmpneu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdfcmpltu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdfcmpgeu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdfcmpgtu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdfcmpleu

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfcmpeq
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfcmpne
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfcmplt
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfcmpge
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfcmpgt
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfcmple
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfcmpequ
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfcmpneu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfcmpltu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfcmpgeu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfcmpgtu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfcmpleu

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifddcmpeq
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifddcmpne
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifddcmplt
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifddcmpge
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifddcmpgt
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifddcmple
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifddcmpequ
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifddcmpneu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifddcmpltu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifddcmpgeu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifddcmpgtu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifddcmpleu

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddcmpeq
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddcmpne
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddcmplt
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddcmpge
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddcmpgt
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddcmple
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddcmpequ
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddcmpneu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddcmpltu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddcmpgeu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddcmpgtu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddcmpleu

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdecmpeq
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdecmpne
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdecmplt
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdecmpge
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdecmpgt
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdecmple
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdecmpequ
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdecmpneu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdecmpltu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdecmpgeu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdecmpgtu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ifdecmpleu

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::decmpeq
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::decmpne
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::decmplt
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::decmpge
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::decmpgt
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::decmple
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::decmpequ
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::decmpneu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::decmpltu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::decmpgeu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::decmpgtu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::decmpleu

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfRegLoad
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddRegLoad
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deRegLoad
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfRegStore
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddRegStore
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deRegStore

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfternary
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddternary
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deternary

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfexp
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddexp
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deexp
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfnint
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddnint
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::denint
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfsqrt
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddsqrt
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::desqrt

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfcos
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddcos
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::decos
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfsin
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddsin
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::desin
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dftan
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddtan
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::detan

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfcosh
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddcosh
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::decosh
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfsinh
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddsinh
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::desinh
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dftanh
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddtanh
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::detanh

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfacos
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddacos
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deacos
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfasin
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddasin
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deasin
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfatan
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddatan
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deatan

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfatan2
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddatan2
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deatan2
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dflog
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddlog
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::delog
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dffloor
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddfloor
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::defloor
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfceil
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddceil
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deceil
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfmax
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddmax
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::demax
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfmin
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddmin
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::demin

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfInsExp
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddInsExp
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::deInsExp

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ddclean
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::declean

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdload
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdloadi
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdstore
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdstorei

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2zd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zd2pd

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdsleLoad
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdslsLoad
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdstsLoad

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdsleLoadi
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdslsLoadi
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdstsLoadi

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdsleStore
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdslsStore
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdstsStore

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdsleStorei
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdslsStorei
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdstsStorei

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zd2zdsle
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zd2zdsls
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zd2zdsts

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdsle2pd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdsls2pd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdsts2pd

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdsle2zd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdsls2zd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zdsts2zd

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2zdsls
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2zdslsSetSign
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2zdsts
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2zdstsSetSign

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zd2df
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::df2zd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zd2dd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dd2zd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zd2de
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::de2zd

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zd2dfAbs
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zd2ddAbs
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::zd2deAbs

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::df2zdSetSign
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dd2zdSetSign
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::de2zdSetSign

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::df2zdClean
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dd2zdClean
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::de2zdClean

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::udLoad
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::udslLoad
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::udstLoad

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::udLoadi
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::udslLoadi
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::udstLoadi

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::udStore
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::udslStore
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::udstStore

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::udStorei
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::udslStorei
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::udstStorei

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2ud
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2udsl
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2udst

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::udsl2ud
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::udst2ud

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ud2pd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::udsl2pd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::udst2pd

   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdload
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdloadi
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdstore
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdstorei
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdadd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdsub
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdmul
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pddiv
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdrem
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdneg
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdabs
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdshr
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdshl
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdshrSetSign
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdshlSetSign
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdshlOverflow
   TR::TreeEvaluator::badILOpEvaluator,          // TR::pdchk
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2i
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2iu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2iOverflow
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::i2pd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::iu2pd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2l
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2lu
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2lOverflow
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::l2pd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::lu2pd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2f
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2d
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::f2pd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::d2pd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdcmpeq
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdcmpne
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdcmplt
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdcmpge
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdcmpgt
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdcmple
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdclean
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdclear
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdclearSetSign
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdSetSign
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pdModifyPrecision
   TR::TreeEvaluator::badILOpEvaluator,          // TR::countDigits
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2df
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2dfAbs
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::df2pd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::df2pdSetSign
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::df2pdClean
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2dd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2ddAbs
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dd2pd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dd2pdSetSign
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dd2pdClean
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2de
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::pd2deAbs
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::de2pd
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::de2pdSetSign
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::de2pdClean
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::BCDCHK
   TR::TreeEvaluator::badILOpEvaluator,          // TR::ircload (J9)
   TR::TreeEvaluator::badILOpEvaluator,          // TR::irsload (J9)
   TR::TreeEvaluator::badILOpEvaluator,          // TR::iruiload (J9)
   TR::TreeEvaluator::badILOpEvaluator,          // TR::iriload (J9)
   TR::TreeEvaluator::badILOpEvaluator,          // TR::irulload (J9)
   TR::TreeEvaluator::badILOpEvaluator,          // TR::irlload (J9)
   TR::TreeEvaluator::badILOpEvaluator,          // TR::irsstore (J9)
   TR::TreeEvaluator::badILOpEvaluator,          // TR::iristore (J9)
   TR::TreeEvaluator::badILOpEvaluator,          // TR::irlstore (J9)
