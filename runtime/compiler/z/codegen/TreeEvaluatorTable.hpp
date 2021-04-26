/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#define _zdloadEvaluator TR::TreeEvaluator::pdloadEvaluator
#define _zdloadiEvaluator TR::TreeEvaluator::pdloadEvaluator
#define _zdstoreEvaluator TR::TreeEvaluator::pdstoreEvaluator
#define _zdstoreiEvaluator TR::TreeEvaluator::pdstoreEvaluator
#define _pd2zdEvaluator TR::TreeEvaluator::pd2zdEvaluator
#define _zd2pdEvaluator TR::TreeEvaluator::zd2pdEvaluator
#define _zdsleLoadEvaluator TR::TreeEvaluator::pdloadEvaluator
#define _zdslsLoadEvaluator TR::TreeEvaluator::pdloadEvaluator
#define _zdstsLoadEvaluator TR::TreeEvaluator::pdloadEvaluator
#define _zdsleLoadiEvaluator TR::TreeEvaluator::pdloadEvaluator
#define _zdslsLoadiEvaluator TR::TreeEvaluator::pdloadEvaluator
#define _zdstsLoadiEvaluator TR::TreeEvaluator::pdloadEvaluator
#define _zdsleStoreEvaluator TR::TreeEvaluator::pdstoreEvaluator
#define _zdslsStoreEvaluator TR::TreeEvaluator::pdstoreEvaluator
#define _zdstsStoreEvaluator TR::TreeEvaluator::pdstoreEvaluator
#define _zdsleStoreiEvaluator TR::TreeEvaluator::pdstoreEvaluator
#define _zdslsStoreiEvaluator TR::TreeEvaluator::pdstoreEvaluator
#define _zdstsStoreiEvaluator TR::TreeEvaluator::pdstoreEvaluator
#define _zd2zdsleEvaluator TR::TreeEvaluator::zdsle2zdEvaluator
#define _zd2zdslsEvaluator TR::TreeEvaluator::zd2zdslsEvaluator
#define _zd2zdstsEvaluator TR::TreeEvaluator::zd2zdslsEvaluator
#define _zdsle2pdEvaluator TR::TreeEvaluator::unImpOpEvaluator
#define _zdsls2pdEvaluator TR::TreeEvaluator::zdsls2pdEvaluator
#define _zdsts2pdEvaluator TR::TreeEvaluator::zdsls2pdEvaluator
#define _zdsle2zdEvaluator TR::TreeEvaluator::zdsle2zdEvaluator
#define _zdsls2zdEvaluator TR::TreeEvaluator::zdsls2zdEvaluator
#define _zdsts2zdEvaluator TR::TreeEvaluator::zdsls2zdEvaluator
#define _pd2zdslsEvaluator TR::TreeEvaluator::pd2zdslsEvaluator
#define _pd2zdslsSetSignEvaluator TR::TreeEvaluator::pd2zdslsEvaluator
#define _pd2zdstsEvaluator TR::TreeEvaluator::pd2zdslsEvaluator
#define _pd2zdstsSetSignEvaluator TR::TreeEvaluator::pd2zdslsEvaluator
#define _udLoadEvaluator TR::TreeEvaluator::pdloadEvaluator
#define _udslLoadEvaluator TR::TreeEvaluator::pdloadEvaluator
#define _udstLoadEvaluator TR::TreeEvaluator::pdloadEvaluator
#define _udLoadiEvaluator TR::TreeEvaluator::pdloadEvaluator
#define _udslLoadiEvaluator TR::TreeEvaluator::pdloadEvaluator
#define _udstLoadiEvaluator TR::TreeEvaluator::pdloadEvaluator
#define _udStoreEvaluator TR::TreeEvaluator::pdstoreEvaluator
#define _udslStoreEvaluator TR::TreeEvaluator::pdstoreEvaluator
#define _udstStoreEvaluator TR::TreeEvaluator::pdstoreEvaluator
#define _udStoreiEvaluator TR::TreeEvaluator::pdstoreEvaluator
#define _udslStoreiEvaluator TR::TreeEvaluator::pdstoreEvaluator
#define _udstStoreiEvaluator TR::TreeEvaluator::pdstoreEvaluator
#define _pd2udEvaluator TR::TreeEvaluator::pd2udEvaluator
#define _pd2udslEvaluator TR::TreeEvaluator::pd2udslEvaluator
#define _pd2udstEvaluator TR::TreeEvaluator::pd2udslEvaluator
#define _udsl2udEvaluator TR::TreeEvaluator::unImpOpEvaluator
#define _udst2udEvaluator TR::TreeEvaluator::unImpOpEvaluator
#define _ud2pdEvaluator TR::TreeEvaluator::ud2pdEvaluator
#define _udsl2pdEvaluator TR::TreeEvaluator::udsl2pdEvaluator
#define _udst2pdEvaluator TR::TreeEvaluator::udsl2pdEvaluator
#define _pdloadEvaluator TR::TreeEvaluator::pdloadEvaluator
#define _pdloadiEvaluator TR::TreeEvaluator::pdloadEvaluator
#define _pdstoreEvaluator TR::TreeEvaluator::pdstoreEvaluator
#define _pdstoreiEvaluator TR::TreeEvaluator::pdstoreEvaluator
#define _pdaddEvaluator TR::TreeEvaluator::pdaddEvaluator
#define _pdsubEvaluator TR::TreeEvaluator::pdsubEvaluator
#define _pdmulEvaluator TR::TreeEvaluator::pdmulEvaluator
#define _pddivEvaluator TR::TreeEvaluator::pddivremEvaluator
#define _pdremEvaluator TR::TreeEvaluator::pddivremEvaluator
#define _pdnegEvaluator TR::TreeEvaluator::pdnegEvaluator
#define _pdabsEvaluator TR::TreeEvaluator::unImpOpEvaluator
#define _pdshrEvaluator TR::TreeEvaluator::pdshrEvaluator
#define _pdshlEvaluator TR::TreeEvaluator::pdshlEvaluator
#define _pdshrSetSignEvaluator TR::TreeEvaluator::unImpOpEvaluator
#define _pdshlSetSignEvaluator TR::TreeEvaluator::unImpOpEvaluator
#define _pdshlOverflowEvaluator TR::TreeEvaluator::pdshlEvaluator
#define _pdchkEvaluator TR::TreeEvaluator::pdchkEvaluator
#define _pd2iEvaluator TR::TreeEvaluator::pd2iEvaluator
#define _pd2iOverflowEvaluator TR::TreeEvaluator::pd2iEvaluator
#define _pd2iuEvaluator TR::TreeEvaluator::pd2iEvaluator
#define _i2pdEvaluator TR::TreeEvaluator::i2pdEvaluator
#define _iu2pdEvaluator TR::TreeEvaluator::unImpOpEvaluator
#define _pd2lEvaluator TR::TreeEvaluator::pd2lEvaluator
#define _pd2lOverflowEvaluator TR::TreeEvaluator::pd2lEvaluator
#define _pd2luEvaluator TR::TreeEvaluator::pd2lEvaluator
#define _l2pdEvaluator TR::TreeEvaluator::l2pdEvaluator
#define _lu2pdEvaluator TR::TreeEvaluator::unImpOpEvaluator
#define _pd2fEvaluator TR::TreeEvaluator::unImpOpEvaluator
#define _pd2dEvaluator TR::TreeEvaluator::unImpOpEvaluator
#define _f2pdEvaluator TR::TreeEvaluator::unImpOpEvaluator
#define _d2pdEvaluator TR::TreeEvaluator::unImpOpEvaluator
#define _pdcmpeqEvaluator TR::TreeEvaluator::pdcmpeqEvaluator
#define _pdcmpneEvaluator TR::TreeEvaluator::pdcmpneEvaluator
#define _pdcmpltEvaluator TR::TreeEvaluator::pdcmpltEvaluator
#define _pdcmpgeEvaluator TR::TreeEvaluator::pdcmpgeEvaluator
#define _pdcmpgtEvaluator TR::TreeEvaluator::pdcmpgtEvaluator
#define _pdcmpleEvaluator TR::TreeEvaluator::pdcmpleEvaluator
#define _pdcleanEvaluator TR::TreeEvaluator::unImpOpEvaluator
#define _pdclearEvaluator TR::TreeEvaluator::pdclearEvaluator
#define _pdclearSetSignEvaluator TR::TreeEvaluator::pdclearEvaluator
#define _pdSetSignEvaluator TR::TreeEvaluator::pdSetSignEvaluator
#define _pdModifyPrecisionEvaluator TR::TreeEvaluator::pdModifyPrecisionEvaluator
#define _countDigitsEvaluator TR::TreeEvaluator::countDigitsEvaluator
#define _BCDCHKEvaluator TR::TreeEvaluator::BCDCHKEvaluator

#include "z/codegen/OMRTreeEvaluatorTable.hpp"
