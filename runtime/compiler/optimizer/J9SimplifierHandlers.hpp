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

#ifndef J9_SIMPLIFIERHANDLERS_INCL
#define J9_SIMPLIFIERHANDLERS_INCL

TR::Node * zdloadSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * zdstsStoreSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * zdsleStoreSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * zd2zdsleSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pd2udSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ud2pdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * udsx2pdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pd2udslSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * zdsle2pdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * udsl2udSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * zdsls2zdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pd2zdslsSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pd2zdslsSetSignSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * zd2zdslsSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * zdsls2pdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * zdsle2zdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * zd2pdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pd2zdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * zd2dfSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * df2zdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * df2zdSetSignSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pd2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pd2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * i2pdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pdaddSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pdsubSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pdmulSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pddivSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pdshrSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pdloadSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pdSetSignSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pdcleanSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pdclearSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pdclearSetSignSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pdstoreSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * f2pdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pd2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pdshlSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pdshlSetSignSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pdshrSetSignSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pd2dfSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * df2pdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * pdnegSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ifDFPCompareSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * dfp2dfpSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * df2pdSetSignSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * int2dfpSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * dfp2intSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * dfpShiftLeftSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * dfpShiftRightSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * dfpModifyPrecisionSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * dfpSetSignSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * dfpFloorSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * dfpAddSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * dfpMulSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * dfpDivSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);

#endif
