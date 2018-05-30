/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#ifndef J9_DATATYPES_INLINES_INCL
#define J9_DATATYPES_INLINES_INCL

#include "il/J9DataTypes.hpp"
#include "il/OMRDataTypes_inlines.hpp"

bool
J9::DataType::isFloatingPoint()
   {
   return self()->isBFPorHFP() || self()->isDFP();
   }

bool
J9::DataType::isEmbeddedSign()
   {
   return self()->isZonedEmbeddedSign() || self()->isAnyPacked();
   }

bool
J9::DataType::isUnicodeSignedType()
   {
   return self()->isUnicodeSeparateSign();
   }

bool
J9::DataType::isSeparateSignType()
   {
   return self()->isZonedSeparateSign() || self()->isUnicodeSeparateSign();
   }

bool
J9::DataType::isLeadingSignType()
   {
   return self()->isZonedLeadingSign() || self()->isUnicodeLeadingSign();
   }

bool
J9::DataType::isSeparateSign()
   {
   return self()->isZonedSeparateSign() || self()->isUnicodeSeparateSign();
   }

bool
J9::DataType::isLeadingSeparateSign()
   {
   return self()->getDataType() == TR::ZonedDecimalSignLeadingSeparate || self()->getDataType() == TR::UnicodeDecimalSignLeading;
   }

bool
J9::DataType::isTrailingSeparateSign()
   {
   return self()->getDataType() == TR::ZonedDecimalSignTrailingSeparate || self()->getDataType() == TR::UnicodeDecimalSignTrailing;
   }

bool
J9::DataType::isLeadingSign()
   {
   return self()->isLeadingSeparateSign() || (self()->getDataType() == TR::ZonedDecimalSignLeadingEmbedded);
   }

bool
J9::DataType::isTrailingSign()
   {
   return self()->isTrailingSeparateSign() || (self()->getDataType() == TR::PackedDecimal) || (self()->getDataType() == TR::ZonedDecimal);
   }

bool
J9::DataType::isSignless()
   {
   return self()->getDataType() == TR::UnicodeDecimal;
   }

bool
J9::DataType::hasExposedConstantAddress()
   {
   return self()->isBCD() || self()->isAggregate();
   }

bool
J9::DataType::isSupportedRawSign(int32_t sign)
    {
    TR_RawBCDSignCode rawSign = TR::DataType::getSupportedRawSign(sign);
    if (rawSign == raw_bcd_sign_unknown)
        return false;
    else
        return true;
    }

#endif // J9_DATATYPES_INLINES_INCL
