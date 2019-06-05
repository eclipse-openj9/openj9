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

#include "il/J9DataTypes.hpp"

#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "il/DataTypes.hpp"
#include "infra/Assert.hpp"

namespace J9 {

#define TR_Bad TR::BadILOp

static TR::ILOpCodes conversionMapOMR2TR[TR::NumOMRTypes][TR::NumTypes-TR::NumOMRTypes] =
//                   DecFlt    DecDbl    DecLD     PackedDec ZonedDec ZDecSLE ZDecSLS ZDecSTS UniDec  UniDecSL UniDecST
   {
/* NoType */       { TR_Bad,   TR_Bad,   TR_Bad,   TR_Bad,   TR_Bad,  TR_Bad, TR_Bad, TR_Bad, TR_Bad, TR_Bad,  TR_Bad },  // NoType
/* Int8 */         { TR::b2df, TR::b2dd, TR::b2de, TR_Bad,   TR_Bad,  TR_Bad, TR_Bad, TR_Bad, TR_Bad, TR_Bad,  TR_Bad },  // Int8
/* Int16 */        { TR::s2df, TR::s2dd, TR::s2de, TR_Bad,   TR_Bad,  TR_Bad, TR_Bad, TR_Bad, TR_Bad, TR_Bad,  TR_Bad },  // Int16
/* Int32 */        { TR::i2df, TR::i2dd, TR::i2de, TR::i2pd, TR_Bad,  TR_Bad, TR_Bad, TR_Bad, TR_Bad, TR_Bad,  TR_Bad },  // Int32
/* Int64 */        { TR::l2df, TR::l2dd, TR::l2de, TR::l2pd, TR_Bad,  TR_Bad, TR_Bad, TR_Bad, TR_Bad, TR_Bad,  TR_Bad },  // Int64
/* Float */        { TR::f2df, TR::f2dd, TR::f2de, TR::f2pd, TR_Bad,  TR_Bad, TR_Bad, TR_Bad, TR_Bad, TR_Bad,  TR_Bad },  // Float
/* Double */       { TR::d2df, TR::d2dd, TR::d2de, TR::d2pd, TR_Bad,  TR_Bad, TR_Bad, TR_Bad, TR_Bad, TR_Bad,  TR_Bad },  // Double
/* Address */      { TR_Bad,   TR_Bad,   TR_Bad,   TR_Bad,   TR_Bad,  TR_Bad, TR_Bad, TR_Bad, TR_Bad, TR_Bad,  TR_Bad },  // Address
/* VectorInt8 */   { TR_Bad,   TR_Bad,   TR_Bad,   TR_Bad,   TR_Bad,  TR_Bad, TR_Bad, TR_Bad, TR_Bad, TR_Bad,  TR_Bad },  // VectorInt8
/* VectorInt16*/   { TR_Bad,   TR_Bad,   TR_Bad,   TR_Bad,   TR_Bad,  TR_Bad, TR_Bad, TR_Bad, TR_Bad, TR_Bad,  TR_Bad },  // VectorInt16
/* VectorInt32*/   { TR_Bad,   TR_Bad,   TR_Bad,   TR_Bad,   TR_Bad,  TR_Bad, TR_Bad, TR_Bad, TR_Bad, TR_Bad,  TR_Bad },  // VectorInt32
/* VectorInt64*/   { TR_Bad,   TR_Bad,   TR_Bad,   TR_Bad,   TR_Bad,  TR_Bad, TR_Bad, TR_Bad, TR_Bad, TR_Bad,  TR_Bad },  // VectorInt64
/* VectorFloat*/   { TR_Bad,   TR_Bad,   TR_Bad,   TR_Bad,   TR_Bad,  TR_Bad, TR_Bad, TR_Bad, TR_Bad, TR_Bad,  TR_Bad },  // VectorFloat
/* VectorDouble*/  { TR_Bad,   TR_Bad,   TR_Bad,   TR_Bad,   TR_Bad,  TR_Bad, TR_Bad, TR_Bad, TR_Bad, TR_Bad,  TR_Bad }   // VectorDouble
   };

static TR::ILOpCodes conversionMapTR2OMR[TR::NumTypes-TR::NumOMRTypes][TR::NumOMRTypes] =
//                                       No      Int8     Int16    Int32     Int64   Float     Double    Addr     VectorInt8 VectorInt16 VectorInt32 VectorInt64 VectorFloat VectorDouble
   {
/* DecimalFloat */                     { TR_Bad, TR::df2b,TR::df2s,TR::df2i,TR::df2l,TR::df2f, TR::df2d, TR_Bad,  TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad },  // DecimalFloat
/* DecimalDouble */                    { TR_Bad, TR::dd2b,TR::dd2s,TR::dd2i,TR::dd2l,TR::dd2f, TR::dd2d, TR_Bad,  TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad },  // DecimalDouble
/* DecimalLongDouble */                { TR_Bad, TR::de2b,TR::de2s,TR::de2i,TR::de2l,TR::de2f, TR::de2d, TR_Bad,  TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad },  // DecimalLongD
/* PackedDecimal */                    { TR_Bad, TR_Bad,  TR_Bad,  TR::pd2i,TR::pd2l,TR::pd2f, TR::pd2d, TR_Bad,  TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad },  // PackedDecima
/* ZonedDecimal */                     { TR_Bad, TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,   TR_Bad,   TR_Bad,  TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad },  // ZonedDecimal
/* ZonedDecimalSignLeadingEmbedded */  { TR_Bad, TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,   TR_Bad,   TR_Bad,  TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad },  // ZonedDecimal
/* ZonedDecimalSignLeadingSeparate*/   { TR_Bad, TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,   TR_Bad,   TR_Bad,  TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad },  // ZonedDecimal
/* ZonedDecimalSignTrailingSeparate*/  { TR_Bad, TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,   TR_Bad,   TR_Bad,  TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad },  // ZonedDecimal
/* UnicodeDecimal */                   { TR_Bad, TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,   TR_Bad,   TR_Bad,  TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad },  // UnicodeDecim
/* UnicodeDecimalSignLeading */        { TR_Bad, TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,   TR_Bad,   TR_Bad,  TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad },  // UnicodeDecim
/* UnicodeDecimalSignTrailing */       { TR_Bad, TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,   TR_Bad,   TR_Bad,  TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad }   // UnicodeDecim
   };

static TR::ILOpCodes conversionMapTR2TR[TR::NumTypes-TR::NumOMRTypes][TR::NumTypes-TR::NumOMRTypes] =
//                                       DecFlt    DecDbl    DecLD      PackedDec    ZonedDec     ZDecSLE      ZDecSLS      ZDecSTS      UniDec      UniDecSL    UniDecST
   {
/* DecimalFloat */                     { TR_Bad,   TR::df2dd,TR::df2de, TR::df2pd,   TR::df2zd,   TR_Bad,      TR_Bad,      TR_Bad,      TR_Bad,     TR_Bad,     TR_Bad      },
/* DecimalDouble */                    { TR::dd2df,TR_Bad,   TR::dd2de, TR::dd2pd,   TR::dd2zd,   TR_Bad,      TR_Bad,      TR_Bad,      TR_Bad,     TR_Bad,     TR_Bad      },
/* DecimalLongDouble */                { TR::de2df,TR::de2dd,TR_Bad,    TR::de2pd,   TR::de2zd,   TR_Bad,      TR_Bad,      TR_Bad,      TR_Bad,     TR_Bad,     TR_Bad      },
/* PackedDecimal */                    { TR::pd2df,TR::pd2dd,TR::pd2de, TR_Bad,      TR::pd2zd,   TR_Bad,      TR::pd2zdsls,TR::pd2zdsts,TR::pd2ud,  TR::pd2udsl,TR::pd2udst },
/* ZonedDecimal */                     { TR::zd2df,TR::zd2dd,TR::zd2de, TR::zd2pd,   TR_Bad,      TR::zd2zdsle,TR::zd2zdsls,TR::zd2zdsts,TR_Bad,     TR_Bad,     TR_Bad      },
/* ZonedDecimalSignLeadingEmbedded */  { TR_Bad,   TR_Bad,   TR_Bad,    TR_Bad,      TR::zdsle2zd,TR_Bad,      TR_Bad,      TR_Bad,      TR_Bad,     TR_Bad,     TR_Bad      },
/* ZonedDecimalSignLeadingSeparate*/   { TR_Bad,   TR_Bad,   TR_Bad,    TR::zdsls2pd,TR::zdsls2zd,TR_Bad,      TR_Bad,      TR_Bad,      TR_Bad,     TR_Bad,     TR_Bad      },
/* ZonedDecimalSignTrailingSeparate*/  { TR_Bad,   TR_Bad,   TR_Bad,    TR::zdsts2pd,TR::zdsts2zd,TR_Bad,      TR_Bad,      TR_Bad,      TR_Bad,     TR_Bad,     TR_Bad      },
/* UnicodeDecimal */                   { TR_Bad,   TR_Bad,   TR_Bad,    TR::ud2pd,   TR_Bad,      TR_Bad,      TR_Bad,      TR_Bad,      TR_Bad,     TR_Bad,     TR_Bad      },
/* UnicodeDecimalSignLeading */        { TR_Bad,   TR_Bad,   TR_Bad,    TR::udsl2pd, TR_Bad,      TR_Bad,      TR_Bad,      TR_Bad,      TR::udsl2ud,TR_Bad,     TR_Bad      },
/* UnicodeDecimalSignTrailing */       { TR_Bad,   TR_Bad,   TR_Bad,    TR::udst2pd, TR_Bad,      TR_Bad,      TR_Bad,      TR_Bad,      TR::udst2ud,TR_Bad,     TR_Bad      }
   };

#undef TR_Bad

} // namespace J9

const TR_BCDSignCode decimalSignCodeMap[TR_NUM_DECIMAL_CODES] = {
                                                                 bcd_invalid_sign,  // 0
                                                                 bcd_invalid_sign,  // 1
                                                                 bcd_invalid_sign,  // 2
                                                                 bcd_invalid_sign,  // 3
                                                                 bcd_invalid_sign,  // 4
                                                                 bcd_invalid_sign,  // 5
                                                                 bcd_invalid_sign,  // 6
                                                                 bcd_invalid_sign,  // 7
                                                                 bcd_invalid_sign,  // 8
                                                                 bcd_invalid_sign,  // 9
                                                                 bcd_plus,          // a
                                                                 bcd_minus,         // b
                                                                 bcd_plus,          // c
                                                                 bcd_minus,         // d
                                                                 bcd_plus,          // e
                                                                 bcd_unsigned       // f
                                                                };

const TR_RawBCDSignCode rawSignCodeMap[TR_NUM_DECIMAL_CODES] = {
                                                               raw_bcd_sign_unknown,  // 0
                                                               raw_bcd_sign_unknown,  // 1
                                                               raw_bcd_sign_unknown,  // 2
                                                               raw_bcd_sign_unknown,  // 3
                                                               raw_bcd_sign_unknown,  // 4
                                                               raw_bcd_sign_unknown,  // 5
                                                               raw_bcd_sign_unknown,  // 6
                                                               raw_bcd_sign_unknown,  // 7
                                                               raw_bcd_sign_unknown,  // 8
                                                               raw_bcd_sign_unknown,  // 9
                                                               raw_bcd_sign_unknown,  // a
                                                               raw_bcd_sign_unknown,  // b
                                                               raw_bcd_sign_0xc,      // c
                                                               raw_bcd_sign_0xd,      // d
                                                               raw_bcd_sign_unknown,  // e
                                                               raw_bcd_sign_0xf       // f
                                                               };

const TR_RawBCDSignCode bcdToRawSignCodeMap[num_bcd_sign_codes] = {
                                                                  raw_bcd_sign_unknown,
                                                                  raw_bcd_sign_0xc,      // bcd_plus
                                                                  raw_bcd_sign_0xd,      // bcd_minus
                                                                  raw_bcd_sign_0xf       // bcd_unsigned
                                                                  };

const TR_BCDSignCode rawToBCDSignCodeMap[num_raw_bcd_sign_codes] = {
                                                                   bcd_invalid_sign, // raw_bcd_sign_unknown
                                                                   bcd_plus,         // raw_bcd_sign_0xc
                                                                   bcd_minus,        // raw_bcd_sign_0xd
                                                                   bcd_unsigned      // raw_bcd_sign_0xf
                                                                   };

/// `packedDecimalPrecisionToByteLengthMap` is calculated from the formula:
///
///      int32_t n = precision + 1;
///      if (n is odd)
///         byteLength = (n/2) + 1;
///      else
///         byteLength = (n/2);
///
/// EXCEPT for precision=0 that returns a byteLength of zero (a sign code alone
/// does not a number make)
const int32_t packedDecimalPrecisionToByteLengthMap[TR_MAX_DECIMAL_PRECISION+1]
=
// prec 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
       {0,1,2,2,3,3,4,4,5,5,6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16,
// prec 32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63
        17,17,18,18,19,19,20,20,21,21,22,22,23,23,24,24,25,25,26,26,27,27,28,28,29,29,30,30,31,31,32,32};



char *J9::DataType::_TR_RawBCDSignCodeNames[num_raw_bcd_sign_codes] =
   {
   "raw_bcd_sign_unknown",
   "0xc",
   "0xd",
   "0xf",
   };

int32_t J9::DataType::_TR_RawBCDSignCodeValues[num_raw_bcd_sign_codes] =
   {
   TR::DataType::getInvalidSignCode(),   // bcd_sign_unknown
   0xc,                                  // raw_bcd_sign_0xc
   0xd,                                  // raw_bcd_sign_0xd
   0xf,                                  // raw_bcd_sign_0xf
   };

char *J9::DataType::_TR_BCDSignCodeNames[num_bcd_sign_codes] =
   {
   "bcd_sign_unknown",
   "plus",
   "minus",
   "unsigned",
   };




TR::ILOpCodes
J9::DataType::getDataTypeConversion(TR::DataType t1, TR::DataType t2)
   {
   TR_ASSERT(t1 < TR::NumTypes, "conversion opcode from unexpected datatype %s requested", t1.toString());
   TR_ASSERT(t2 < TR::NumTypes, "conversion opcode to unexpected data type %s requested", t2.toString());
   if (t1 < TR::NumOMRTypes)
      {
      if (t2 < TR::NumOMRTypes)
         return OMR::DataType::getDataTypeConversion(t1, t2);
      else
         return J9::conversionMapOMR2TR[t1][t2 - TR::NumOMRTypes];
      }
   else
      {
      if (t2 < TR::NumOMRTypes)
         return J9::conversionMapTR2OMR[t1 - TR::NumOMRTypes][t2];
      else
         return J9::conversionMapTR2TR[t1 - TR::NumOMRTypes][t2 - TR::NumOMRTypes];
      }
   }

static int32_t J9DataTypeSizes[] =
   {
   4,    // TR::DecimalFloat
   8,    // TR::DecimalDouble
   16,   // TR::DecimalLongDouble
   0,    // TR::PackedDecimal   -- The size of a BCD type can vary. The actual size for a particular symbol is in _size
   0,    // TR::ZonedDecimal
   0,    // TR::ZonedDecimalSignLeadingEmbedded
   0,    // TR::ZonedDecimalSignLeadingSeparate
   0,    // TR::ZonedDecimalSignTrailingSeparate
   0,    // TR::UnicodeDecimal
   0,    // TR::UnicodeDecimalSignLeading
   0     // TR::UnicodeDecimalSignTrailing
   };

static_assert(TR::LastJ9Type - TR::FirstJ9Type + 1 == (sizeof(J9DataTypeSizes) / sizeof(J9DataTypeSizes[0])), "J9DataTypeSizes is not the correct size");

const int32_t
J9::DataType::getSize(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumTypes, "dataTypeSizeMap called on unrecognized data type");
   if (dt < TR::FirstJ9Type)
      return OMR::DataType::getSize(dt);
   else
      return J9DataTypeSizes[dt - TR::FirstJ9Type];
   }

void
J9::DataType::setSize(TR::DataType dt, int32_t newSize)
   {
   TR_ASSERT(dt < TR::NumTypes, "setDataTypeSizeInMap called on unrecognized data type");
   if (dt < TR::FirstJ9Type)
      OMR::DataType::setSize(dt, newSize);
   else
      J9DataTypeSizes[dt - TR::FirstJ9Type] = newSize;
   }


static const char * J9DataTypeNames[] =
   {
   "DecimalFloat",
   "DecimalDouble",
   "DecimalLongDouble",
   "PackedDecimal",
   "ZonedDecimal",
   "ZonedDecimalSignLeadingEmbedded",
   "ZonedDecimalSignLeadingSeparate",
   "ZonedDecimalSignTrailingSeparate",
   "UnicodeDecimal",
   "UnicodeDecimalSignLeading",
   "UnicodeDecimalSignTrailing"
   };

static_assert(TR::LastJ9Type - TR::FirstJ9Type + 1 == (sizeof(J9DataTypeNames) / sizeof(J9DataTypeNames[0])), "J9DataTypeNames is not the correct size");

const char *
J9::DataType::getName(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumTypes, "Name requested for unknown datatype");
   if (dt < TR::FirstJ9Type)
      return OMR::DataType::getName(dt);
   else
      return J9DataTypeNames[dt - TR::FirstJ9Type];
   }


static const char *J9DataTypePrefixes[] =
   {
   "DF",    // TR::DecimalFloat
   "DD",    // TR::DecimalDouble
   "DE",    // TR::DecimalLongDouble
   "PD",    // TR::PackedDecimal
   "ZD",    // TR::ZonedDecimal
   "ZDSLE", // TR::ZonedDecimalSignLeadingEmbedded
   "ZDSLS", // TR::ZonedDecimalSignLeadingSeparate
   "ZDSTS", // TR::ZonedDecimalSignTrailingSeparate
   "UD",    // TR::UnicodeDecimal
   "UDSL",  // TR::UnicodeDecimalSignLeading
   "UDST"   // TR::UnicodeDecimalSignTrailing"
   };

static_assert(TR::LastJ9Type - TR::FirstJ9Type + 1 == (sizeof(J9DataTypePrefixes) / sizeof(J9DataTypePrefixes[0])), "J9DataTypePrefixes is not the correct size");

const char *
J9::DataType::getPrefix(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumTypes, "Prefix requested for unknown datatype");
   if (dt < TR::FirstJ9Type)
      return OMR::DataType::getPrefix(dt);
   else
      return J9DataTypePrefixes[dt - TR::FirstJ9Type];
   }


bool
J9::DataType::isValidZonedDigit(uint8_t data)
   {
   uint8_t zone = data & 0xF0;
   uint8_t digit = data & 0x0F;
   if (zone == TR::DataType::getZonedCode() && digit <= 9)
      return true;
   else
      return false;
   }

bool
J9::DataType::isValidUnicodeDigit(uint8_t dataHi, uint8_t dataLo)
   {
   if (dataHi == TR::DataType::getUnicodeZeroCodeHigh() &&         // 00
       (dataLo&0xF0) == TR::DataType::getUnicodeZeroCodeLow() &&   // 30
       (dataLo&0x0F) <= 9)                                         //  0->9
      {
      return true;
      }
   else
      {
      return false;
      }
   }

bool
J9::DataType::isValidZonedSeparateSign(uint8_t sign)
   {
   if (sign == TR::DataType::getZonedSeparatePlus() || sign == TR::DataType::getZonedSeparateMinus())
      return true;
   else
      return false;
   }

bool
J9::DataType::isValidUnicodeSeparateSign(uint8_t signHi, uint8_t signLo)
   {
   uint16_t sign = (signHi<<8)|signLo;
   if (sign == TR::DataType::getNationalSeparatePlus() || sign == TR::DataType::getNationalSeparateMinus())
      return true;
   else
      return false;
   }

bool
J9::DataType::isValidZonedData(char  *lit, int32_t start, int32_t end)
   {
   if (start > end)
      return false;

   for (int32_t i = start; i <= end; i++)
      {
      if (!TR::DataType::isValidZonedDigit(lit[i]))
         return false;
      }
   return true;
   }

bool
J9::DataType::isValidUnicodeData(char  *lit, int32_t start, int32_t end)
   {
   if (start > end)
      return false;

   for (int32_t i = start; i <= end; i+=2)
      {
      uint8_t dataHi = lit[i];
      uint8_t dataLo = lit[i+1];
      if (!TR::DataType::isValidUnicodeDigit(dataHi, dataLo))
         {
         return false;
         }
      }
   return true;
   }

bool
J9::DataType::isValidBCDLiteral(char *lit, size_t litSize, TR::DataType dt, bool isEvenPrecision)
   {
   if (dt.isAnyZoned())
      {
      int32_t dataStart = 0;
      int32_t dataEnd   = litSize-1;
      if (dt.isZonedEmbeddedSign())
         {
         uint8_t signByte = 0;
         if (dt.isZonedLeadingSign())
            {
            signByte = lit[dataStart];
            dataStart += TR::DataType::getZonedElementSize();
            }
         else
            {
            signByte = lit[dataEnd];
            dataEnd -= TR::DataType::getZonedElementSize();
            }

         uint8_t sign = (signByte & 0xF0) >> 4;
         uint8_t digit = signByte & 0x0F;
         if (!TR::DataType::isValidEmbeddedSign(sign))
            return false;
         else if (digit > 9)  // digit has to be in range 0->0xf
            return false;
         else if (litSize == 1) // just verified single byte above so we are done and lit is valid
            return true;
         }
      else if (dt.isZonedLeadingSepSign())
         {
         if (!TR::DataType::isValidZonedSeparateSign(lit[dataStart]))
            return false;
         dataStart += TR::DataType::getZonedSignSize();
         }
      else if (dt.isZonedTrailingSepSign())
         {
         if (!TR::DataType::isValidZonedSeparateSign(lit[dataEnd]))
            return false;
         dataEnd -= TR::DataType::getZonedSignSize();
         }
      else
         {
         TR_ASSERT(false,"unexpected zoned dt %s\n",dt.toString());
         return false;
         }
      return TR::DataType::isValidZonedData(lit, dataStart, dataEnd);
      }
   else if (dt.isAnyUnicode())
      {
      int32_t dataStart = 0;
      int32_t dataEnd = litSize-1;
      if (dt.isUnicodeLeadingSign())
         {
         uint8_t signHi = lit[dataStart];
         uint8_t signLo = lit[dataStart+1];
         if (!TR::DataType::isValidUnicodeSeparateSign(signHi, signLo))
            return false;
         dataStart += TR::DataType::getUnicodeSignSize();
         }
      else if (dt.isUnicodeTrailingSign())
         {
         uint8_t signHi = lit[dataEnd-1];
         uint8_t signLo = lit[dataEnd];
         if (!TR::DataType::isValidUnicodeSeparateSign(signHi, signLo))
            return false;
         dataEnd -= TR::DataType::getUnicodeSignSize();
         }
      else if (dt == TR::UnicodeDecimal)
         {
         // no sign for this type so nothing extra to do,
         }
      else
         {
         TR_ASSERT(false,"unexpected unicode dt %s\n",dt.toString());
         return false;
         }

      return TR::DataType::isValidUnicodeData(lit, dataStart, dataEnd);
      }
   else
      {
      if (dt.isAnyPacked())
         {
         return TR::DataType::isValidPackedData(lit, 0, litSize-1, isEvenPrecision);
         }
      else
         {
         return false;
         }
      }
   }

TR::DataType
J9::DataType::getDFPTypeFromPrecision(int32_t precision)
   {
   if (precision < 1 || precision > J9::DataType::getMaxExtendedDFPPrecision())
      return  TR::NoType;
   else if (precision <= J9::DataType::getMaxShortDFPPrecision())
      return  TR::DecimalFloat;
   else if (precision <= J9::DataType::getMaxLongDFPPrecision())
      return  TR::DecimalDouble;
   else
      return  TR::DecimalLongDouble;
   }

bool
J9::DataType::canGetMaxPrecisionFromType()
   {
   switch (self()->getDataType())
      {
      case TR::DecimalFloat:
      case TR::DecimalDouble:
      case TR::DecimalLongDouble:
      case TR::PackedDecimal:
      case TR::ZonedDecimal:
      case TR::ZonedDecimalSignLeadingEmbedded:
      case TR::ZonedDecimalSignLeadingSeparate:
      case TR::ZonedDecimalSignTrailingSeparate:
      case TR::UnicodeDecimal:
      case TR::UnicodeDecimalSignLeading:
      case TR::UnicodeDecimalSignTrailing:
         return true;
      default:
         return OMR::DataType::canGetMaxPrecisionFromType();
      }
   }

int32_t
J9::DataType::getMaxPrecisionFromType()
   {
   switch (self()->getDataType())
      {
      case TR::DecimalFloat: return TR::DataType::getMaxShortDFPPrecision();
      case TR::DecimalDouble: return TR::DataType::getMaxLongDFPPrecision();
      case TR::DecimalLongDouble: return TR::DataType::getMaxExtendedDFPPrecision();
      case TR::PackedDecimal:
      case TR::ZonedDecimal:
      case TR::ZonedDecimalSignLeadingEmbedded:
      case TR::ZonedDecimalSignLeadingSeparate:
      case TR::ZonedDecimalSignTrailingSeparate:
      case TR::UnicodeDecimal:
      case TR::UnicodeDecimalSignLeading:
      case TR::UnicodeDecimalSignTrailing:
         return TR::DataType::getMaxPackedDecimalPrecision();
      default:
         return OMR::DataType::getMaxPrecisionFromType();
      }
   }

int32_t
J9::DataType::separateSignSize()
   {
   switch (self()->getDataType())
      {
      case TR::ZonedDecimalSignLeadingSeparate:
      case TR::ZonedDecimalSignTrailingSeparate:
         return J9::DataType::getZonedSignSize();
      case TR::UnicodeDecimalSignLeading:
      case TR::UnicodeDecimalSignTrailing:
         return J9::DataType::getUnicodeSignSize();
      default:
         return 0;
      }
   }

int32_t
J9::DataType::getLeftMostByte(TR::DataType dt, int32_t dataSize)
   {
   int32_t leftMostByte = 0;
   switch(dt)
      {
      // for unsigned,embedded,extFloat and leading separate sign types the left most data byte equals the dataSize
      case TR::PackedDecimal:                    // embedded
         leftMostByte = dataSize;
         break;
      // for unsigned,embedded,extFloat and leading separate sign types the left most data byte equals the dataSize
      case TR::UnicodeDecimal:                   // unsigned
      case TR::ZonedDecimal:                     // embedded
      case TR::ZonedDecimalSignLeadingEmbedded:  // embedded
      case TR::ZonedDecimalSignLeadingSeparate:  // leading
      case TR::UnicodeDecimalSignLeading:        // leading
         leftMostByte = dataSize;
         break;
      case TR::ZonedDecimalSignTrailingSeparate:
         leftMostByte = dataSize + TR::DataType::getZonedSignSize();
         break;
      case TR::UnicodeDecimalSignTrailing:
         leftMostByte = dataSize + TR::DataType::getUnicodeSignSize();
         break;
      default:
         TR_ASSERT(false,"unknown BCD type %s\n",dt.toString());
      }
   return leftMostByte;
   }

int32_t
J9::DataType::getSignCodeOffset(TR::DataType dt, int32_t size)
   {
   int32_t offset = 0;
   switch(dt)
      {
      case TR::PackedDecimal:
         offset = size - TR::DataType::getPackedSignSize();
         break;
      case TR::ZonedDecimal:
      case TR::ZonedDecimalSignTrailingSeparate:
         offset = size - TR::DataType::getZonedSignSize();
         break;
      case TR::UnicodeDecimalSignTrailing:
         offset = size - TR::DataType::getUnicodeSignSize();
         break;
      case TR::ZonedDecimalSignLeadingEmbedded:
      case TR::ZonedDecimalSignLeadingSeparate:
      case TR::UnicodeDecimalSignLeading:
         offset = 0;
         break;
      case TR::UnicodeDecimal:
         TR_ASSERT(false,"TR::UnicodeDecimal type does not have a sign code\n");
         offset = 0;
         break;
      default:
         TR_ASSERT(false,"unknown BCD type %s\n",dt.toString());
      }
   return offset;
   }

TR_SignCodeSize
J9::DataType::getSignCodeSize(TR::DataType dt)
   {
   TR_ASSERT(dt.isBCD(),"getSignCodeSize() only valid for bcd types\n");
   TR_SignCodeSize size = UnknownSignCodeSize;
   switch (dt)
      {
      case TR::PackedDecimal:
         size = EmbeddedHalfByte;
         break;
      case TR::ZonedDecimal:
      case TR::ZonedDecimalSignLeadingEmbedded:
         size = EmbeddedHalfByte;
         break;
      case TR::ZonedDecimalSignTrailingSeparate:
      case TR::ZonedDecimalSignLeadingSeparate:
         size = SeparateOneByte;
         break;
      case TR::UnicodeDecimalSignTrailing:
      case TR::UnicodeDecimalSignLeading:
         size = SeparateTwoByte;
         break;
      case TR::UnicodeDecimal:
         size = UnknownSignCodeSize;
         break;
      default:
         TR_ASSERT_FATAL(false, "Unknown sign code BCD type"); 
         break; 
      }
   return size;
   }

TR_BCDSignCode
J9::DataType::getNormalizedSignCode(TR::DataType dt, int32_t rawSignCode)
   {
   TR_ASSERT(dt.isBCD(),"getNormalizedSignCode() only valid for bcd types\n");
   TR_BCDSignCode signCode = bcd_invalid_sign;
   switch(dt)
      {
      case TR::PackedDecimal:
         {
         if (TR::DataType::embeddedSignCodeIsInRange(rawSignCode))
            return decimalSignCodeMap[rawSignCode];
         else
            TR_ASSERT(rawSignCode == TR::DataType::getIgnoredSignCode(),"invalid embedded rawSignCode of 0x%x for datatype %s\n",rawSignCode,dt.toString());
         break;
         }
      case TR::ZonedDecimal:
      case TR::ZonedDecimalSignLeadingEmbedded:
         {
         if (TR::DataType::embeddedSignCodeIsInRange(rawSignCode))
            return decimalSignCodeMap[rawSignCode];
         else
            TR_ASSERT(rawSignCode == TR::DataType::getIgnoredSignCode(),"invalid embedded rawSignCode of 0x%x for datatype %s\n",rawSignCode,dt.toString());
         break;
         }
      case TR::ZonedDecimalSignTrailingSeparate:
      case TR::ZonedDecimalSignLeadingSeparate:
         {
         if (rawSignCode == TR::DataType::getZonedSeparatePlus())
            return bcd_plus;
         else if (rawSignCode == TR::DataType::getZonedSeparateMinus())
            return bcd_minus;
         else
            TR_ASSERT(rawSignCode == TR::DataType::getIgnoredSignCode(),"invalid zoned decimal separate rawSignCode of 0x%x for datatype %s\n",rawSignCode,dt.toString());
         break;
         }
      case TR::UnicodeDecimalSignTrailing:
      case TR::UnicodeDecimalSignLeading:
         {
         if (rawSignCode == TR::DataType::getNationalSeparatePlus())
            return bcd_plus;
         else if (rawSignCode == TR::DataType::getNationalSeparateMinus())
            return bcd_minus;
         else
            TR_ASSERT(rawSignCode == TR::DataType::getIgnoredSignCode(),"invalid unicode decimal separate rawSignCode of 0x%x for datatype %s\n",rawSignCode,dt.toString());
         break;
         }
      case TR::UnicodeDecimal:
         {
         return bcd_unsigned;
         break;
         }
      default:
         TR_ASSERT_FATAL(false,"datatype %s not handled yet in getNormalizedSignCode\n",dt.toString());
      }
   return signCode;
   }

int32_t
J9::DataType::getSizeFromBCDPrecision(TR::DataType dt, int32_t precision)
   {
   TR_ASSERT(dt.isBCD(),"getSizeFromBCDPrecision() only valid for bcd types\n");
   int32_t size = 0;
   switch(dt)
      {
      case TR::PackedDecimal:
         size = TR::DataType::packedDecimalPrecisionToByteLength(precision);
         break;
      case TR::ZonedDecimal:
      case TR::ZonedDecimalSignLeadingEmbedded:
         size = precision * TR::DataType::getZonedElementSize();
         break;
      case TR::ZonedDecimalSignTrailingSeparate:
      case TR::ZonedDecimalSignLeadingSeparate:
         size = (precision * TR::DataType::getZonedElementSize()) + TR::DataType::getZonedSignSize();
         break;
      case TR::UnicodeDecimal:
         size = (precision * TR::DataType::getUnicodeElementSize());
         break;
      case TR::UnicodeDecimalSignTrailing:
      case TR::UnicodeDecimalSignLeading:
         size = (precision * TR::DataType::getUnicodeElementSize()) + TR::DataType::getUnicodeSignSize();
         break;
      default:
         TR_ASSERT(false,"unknown bcd type %s\n",dt.toString());
      }
   return size;
   }

int32_t
J9::DataType::getBCDPrecisionFromSize(TR::DataType dt, int32_t size)
   {
   TR_ASSERT(dt.isBCD(),"getBCDPrecisionFromSize() only valid for bcd types\n");
   int32_t precision = 0;
   switch(dt)
      {
      case TR::PackedDecimal:
         precision = TR::DataType::byteLengthToPackedDecimalPrecisionCeiling(size);
         break;
      case TR::ZonedDecimal:
      case TR::ZonedDecimalSignLeadingEmbedded:
         precision = size;
         break;
      case TR::ZonedDecimalSignTrailingSeparate:
      case TR::ZonedDecimalSignLeadingSeparate:
         precision = size - TR::DataType::getZonedSignSize();
         break;
      case TR::UnicodeDecimal:
         precision = (size / TR::DataType::getUnicodeElementSize());
         break;
      case TR::UnicodeDecimalSignTrailing:
      case TR::UnicodeDecimalSignLeading:
         precision = (size - TR::DataType::getUnicodeSignSize()) / TR::DataType::getUnicodeElementSize();
         break;
      default:
         TR_ASSERT(false,"unknown bcd type %s\n",dt.toString());
      }
   return precision;
   }

uint32_t
J9::DataType::printableToEncodedSign(uint32_t printableSign, TR::DataType dt)
   {
   TR_ASSERT(dt.isBCD(),"printableToEncodedSign() only valid for bcd types\n");
   uint32_t encodedSign = TR::DataType::getInvalidSignCode();
   switch (dt)
      {
      case TR::PackedDecimal:
         if (printableSign == TR::DataType::getBCDPlusChar())
            encodedSign = TR::DataType::getPreferredPlusCode();
         else if (printableSign == TR::DataType::getBCDMinusChar())
            encodedSign = TR::DataType::getPreferredMinusCode();
         else if (printableSign == TR::DataType::getBCDUnsignedChar())
            encodedSign = TR::DataType::getUnsignedCode();
         else
            TR_ASSERT(false,"unknown printable sign 0x%x for dt %s\n",printableSign,dt.toString());
         break;

      case TR::ZonedDecimal:
      case TR::ZonedDecimalSignLeadingEmbedded:
         if (printableSign == TR::DataType::getBCDPlusChar())
            encodedSign = TR::DataType::getPreferredPlusCode();
         else if (printableSign == TR::DataType::getBCDMinusChar())
            encodedSign = TR::DataType::getPreferredMinusCode();
         else if (printableSign == TR::DataType::getBCDUnsignedChar())
            encodedSign = TR::DataType::getUnsignedCode();
         else
            TR_ASSERT(false,"unknown printable sign 0x%x for dt %s\n",printableSign,dt.toString());
         break;

      case TR::ZonedDecimalSignTrailingSeparate:
      case TR::ZonedDecimalSignLeadingSeparate:
         if (printableSign == TR::DataType::getBCDPlusChar() || printableSign == TR::DataType::getBCDUnsignedChar())
            encodedSign = TR::DataType::getZonedSeparatePlus();
         else if (printableSign == TR::DataType::getBCDMinusChar())
            encodedSign = TR::DataType::getZonedSeparateMinus();
         else
            TR_ASSERT(false,"unknown printable sign 0x%x for dt %s\n",printableSign,dt.toString());
         break;

      case TR::UnicodeDecimalSignTrailing:
      case TR::UnicodeDecimalSignLeading:
         if (printableSign == TR::DataType::getBCDPlusChar() || printableSign == TR::DataType::getBCDUnsignedChar())
            encodedSign = TR::DataType::getNationalSeparatePlus();
         else if (printableSign == TR::DataType::getBCDMinusChar())
            encodedSign = TR::DataType::getNationalSeparateMinus();
         else
            TR_ASSERT(false,"unknown printable sign 0x%x for dt %s\n",printableSign,dt.toString());
         break;

      case TR::UnicodeDecimal:
         TR_ASSERT(false,"TR::UnicodeDecimal type (%s) does not have a sign code (printableSign = 0x%x)\n",dt.toString(),printableSign);
         break;

      default:
         TR_ASSERT(false,"unknown bcd type %s\n",dt.toString());
      }
   return encodedSign;
   }

uint32_t
J9::DataType::encodedToPrintableSign(uint32_t encodedSign, TR::DataType dt)
   {
   TR_ASSERT(dt.isBCD(),"encodedToPrintableSign() only valid for bcd types\n");
   uint32_t printableSign = '+';
   switch (dt)
      {
      case TR::PackedDecimal:
         {
         TR_ASSERT(encodedSign >= TR::DataType::getFirstValidSignCode() && encodedSign <= TR::DataType::getLastValidSignCode(),"invalid sign code 0x%x for dt %s\n",encodedSign,dt.toString());
         TR_BCDSignCode normalizedSignCode = decimalSignCodeMap[encodedSign&0xf];
         if (normalizedSignCode == bcd_plus)
            printableSign = TR::DataType::getBCDPlusChar();
         else if (normalizedSignCode == bcd_minus)
            printableSign = TR::DataType::getBCDMinusChar();
         else if (normalizedSignCode == bcd_unsigned)
            printableSign = TR::DataType::getBCDUnsignedChar();
         else
            TR_ASSERT(false,"unknown encoded sign 0x%x for dt %s\n",encodedSign,dt.toString());
         }
         break;

      case TR::ZonedDecimal:
      case TR::ZonedDecimalSignLeadingEmbedded:
         {
         TR_ASSERT(encodedSign >= TR::DataType::getFirstValidSignCode() && encodedSign <= TR::DataType::getLastValidSignCode(),"invalid sign code 0x%x for dt %s\n",encodedSign,dt.toString());
         TR_BCDSignCode normalizedSignCode = decimalSignCodeMap[encodedSign&0xf];
         if (normalizedSignCode == bcd_plus)
            printableSign = TR::DataType::getBCDPlusChar();
         else if (normalizedSignCode == bcd_minus)
            printableSign = TR::DataType::getBCDMinusChar();
         else if (normalizedSignCode == bcd_unsigned)
            printableSign = TR::DataType::getBCDUnsignedChar();
         else
            TR_ASSERT(false,"unknown encoded sign 0x%x for dt %s\n",encodedSign,dt.toString());
         }
         break;
      case TR::ZonedDecimalSignTrailingSeparate:
      case TR::ZonedDecimalSignLeadingSeparate:
         {
         if (encodedSign == TR::DataType::getZonedSeparatePlus())
            printableSign = TR::DataType::getBCDPlusChar();
         else if (encodedSign == TR::DataType::getZonedSeparateMinus())
            printableSign = TR::DataType::getBCDMinusChar();
         else
           TR_ASSERT(false,"unknown encoded sign 0x%x for dt %s\n",encodedSign,dt.toString());
         }
         break;
      case TR::UnicodeDecimalSignTrailing:
      case TR::UnicodeDecimalSignLeading:
         {
         if (encodedSign == TR::DataType::getNationalSeparatePlus())
            printableSign = TR::DataType::getBCDPlusChar();
         else if (encodedSign == TR::DataType::getNationalSeparateMinus())
            printableSign = TR::DataType::getBCDMinusChar();
         else
            TR_ASSERT(false,"unknown encoded sign 0x%x for dt %s\n",encodedSign,dt.toString());
         }
         break;
      case TR::UnicodeDecimal:
         TR_ASSERT(false,"TR::UnicodeDecimal type (%s) does not have a sign code (encodedSign = 0x%x)\n",dt.toString(),encodedSign);
         break;
      default:
         TR_ASSERT(false,"unknown bcd type %s\n",dt.toString());
         break; 
      }
   return printableSign;
   }

uint8_t
J9::DataType::getOneByteBCDFill(TR::DataType dt)
   {
   TR_ASSERT(dt.isBCD(),"getOneByteBCDFill() only valid for bcd types\n");
   uint8_t fill = 0;
   switch (dt)
      {
      case TR::PackedDecimal:
         fill = 0;
         break;
      case TR::ZonedDecimal:
      case TR::ZonedDecimalSignLeadingEmbedded:
      case TR::ZonedDecimalSignTrailingSeparate:
      case TR::ZonedDecimalSignLeadingSeparate:
         fill = TR::DataType::getZonedCode();
         break;
      default:
         TR_ASSERT(false,"type %s does not a have 1 byte fill\n",dt.toString());
      }
   return fill;
   }

uint16_t
J9::DataType::getTwoByteBCDFill(TR::DataType dt)
   {
   TR_ASSERT(dt.isBCD(),"getTwoByteBCDFill() only valid for bcd types\n");
   uint16_t fill = 0;
   switch (dt)
      {
      case TR::UnicodeDecimal:
      case TR::UnicodeDecimalSignTrailing:
      case TR::UnicodeDecimalSignLeading:
         fill = TR::DataType::getUnicodeZeroCode();
         break;
      default:
         TR_ASSERT(false,"type %s does not a have 2 byte fill\n",dt.toString());
      }
   return fill;
   }

TR_DigitSize
J9::DataType::getDigitSize(TR::DataType dt)
   {
   TR_ASSERT(dt.isBCD(),"getDigitSize() only valid for bcd types\n");
   TR_DigitSize size = UnknownDigitSize;
   switch (dt)
      {
      case TR::PackedDecimal:
         size = HalfByteDigit;
         break;
      case TR::ZonedDecimal:
      case TR::ZonedDecimalSignLeadingEmbedded:
      case TR::ZonedDecimalSignTrailingSeparate:
      case TR::ZonedDecimalSignLeadingSeparate:
         size = OneByteDigit;
         break;
      case TR::UnicodeDecimal:
      case TR::UnicodeDecimalSignTrailing:
      case TR::UnicodeDecimalSignLeading:
         size = TwoByteDigit;
         break;
      default:
         TR_ASSERT(false,"unknown bcd type %s\n",dt.toString());
      }
   return size;
   }

int32_t
J9::DataType::bytesToDigits(TR::DataType dt, int32_t bytes)
   {
   int32_t digits = 0;
   switch(dt)
      {
      case TR::PackedDecimal:
         digits = bytes*2;
         break;
      case TR::ZonedDecimal:
      case TR::ZonedDecimalSignLeadingEmbedded:
      case TR::ZonedDecimalSignTrailingSeparate:
      case TR::ZonedDecimalSignLeadingSeparate:
         digits = bytes;
         break;
      case TR::UnicodeDecimal:
      case TR::UnicodeDecimalSignTrailing:
      case TR::UnicodeDecimalSignLeading:
         TR_ASSERT((bytes&0x1) == 0,"bytes (%d) for a unicode type must be an even number\n",bytes);
         digits = bytes/2;
         break;
      default:
         TR_ASSERT(false,"unknown BCD type %s\n",dt.toString());
      }
   return digits;
   }

int32_t
J9::DataType::digitsToBytes(TR::DataType dt, int32_t digits)
   {
   int32_t bytes = 0;
   switch(dt)
      {
      case TR::PackedDecimal:
         TR_ASSERT((digits&0x1) == 0,"digits (%d) for a packed type must be an even number\n",digits);
         bytes = digits/2;
         break;
      case TR::ZonedDecimal:
      case TR::ZonedDecimalSignLeadingEmbedded:
      case TR::ZonedDecimalSignTrailingSeparate:
      case TR::ZonedDecimalSignLeadingSeparate:
         bytes = digits;
         break;
      case TR::UnicodeDecimal:
      case TR::UnicodeDecimalSignTrailing:
      case TR::UnicodeDecimalSignLeading:
         bytes = digits*2;
         break;
      default:
         TR_ASSERT(false,"unknown BCD type %s\n",dt.toString());
      }
   return bytes;
   }

int32_t
J9::DataType::getPreferredPlusSignCode(TR::DataType dt)
   {
   TR_ASSERT(dt.isBCD(),"getPreferredPlusSignCode() only valid for bcd types\n");
   int32_t sign = TR::DataType::getInvalidSignCode();
   switch (dt)
      {
      case TR::PackedDecimal:
         sign = TR::DataType::getPreferredPlusCode();
         break;
      case TR::ZonedDecimal:
      case TR::ZonedDecimalSignLeadingEmbedded:
         sign = TR::DataType::getPreferredPlusCode();
         break;
      case TR::ZonedDecimalSignLeadingSeparate:
      case TR::ZonedDecimalSignTrailingSeparate:
         sign = TR::DataType::getZonedSeparatePlus();
         break;
      case TR::UnicodeDecimalSignLeading:
      case TR::UnicodeDecimalSignTrailing:
         sign = TR::DataType::getNationalSeparatePlus();
         break;
      case TR::UnicodeDecimal:
         sign = TR::DataType::getInvalidSignCode();
         break;
      default:
         TR_ASSERT(false,"unknown bcd type %s\n",dt.toString());
      }
   return sign;
   }

int32_t
J9::DataType::getPreferredMinusSignCode(TR::DataType dt)
   {
   TR_ASSERT(dt.isBCD(),"getPreferredMinusSignCode() only valid for bcd types\n");
   int32_t sign = TR::DataType::getInvalidSignCode();
   switch (dt)
      {
      case TR::PackedDecimal:
         sign = TR::DataType::getPreferredMinusCode();
         break;
      case TR::ZonedDecimal:
      case TR::ZonedDecimalSignLeadingEmbedded:
         sign = TR::DataType::getPreferredMinusCode();
         break;
      case TR::ZonedDecimalSignLeadingSeparate:
      case TR::ZonedDecimalSignTrailingSeparate:
         sign = TR::DataType::getZonedSeparateMinus();
         break;
      case TR::UnicodeDecimalSignLeading:
      case TR::UnicodeDecimalSignTrailing:
         sign = TR::DataType::getNationalSeparateMinus();
         break;
      case TR::UnicodeDecimal:
         sign = TR::DataType::getInvalidSignCode();
         break;
      default:
         TR_ASSERT(false,"unknown bcd type %s\n",dt.toString());
      }
   return sign;
   }

bool
J9::DataType::rawSignIsPositive(TR::DataType dt, int32_t rawSignCode)
   {
   TR_BCDSignCode normalizedSign = TR::DataType::getNormalizedSignCode(dt, rawSignCode);
   return TR::DataType::normalizedSignIsPositive(dt, normalizedSign);
   }

bool
J9::DataType::normalizedSignIsPositive(TR::DataType dt, TR_BCDSignCode normalizedSign)
   {
   if (normalizedSign == bcd_plus || normalizedSign == bcd_unsigned)
      return true;
   else if (normalizedSign == bcd_minus)
      return false;
   else
      TR_ASSERT(false,"invalid sign %d for dt %s\n",normalizedSign,dt.toString());
   return false;
   }

bool
J9::DataType::rawSignIsNegative(TR::DataType dt, int32_t rawSignCode)
   {
   TR_BCDSignCode normalizedSign = TR::DataType::getNormalizedSignCode(dt, rawSignCode);
   return TR::DataType::normalizedSignIsNegative(dt, normalizedSign);
   }

bool
J9::DataType::normalizedSignIsNegative(TR::DataType dt, TR_BCDSignCode normalizedSign)
   {
   if (normalizedSign == bcd_plus || normalizedSign == bcd_unsigned)
      return false;
   else if (normalizedSign == bcd_minus)
      return true;
   else
      TR_ASSERT(false,"invalid sign %d for dt %s\n",normalizedSign,dt.toString());
   return false;
   }

int32_t
J9::DataType::getBCDPrecisionFromString(char *str, TR::DataType dt) // +123 prec=3, 123 prec=3
   {
   int32_t precision = strlen(str);
   if (TR::DataType::isBCDSignChar(str[0]))
      {
      precision -= TR::DataType::getBCDSignCharSize();
      }
   else
      {
      TR_ASSERT(isdigit(str[0]),"expecting a minus/plus sign or a decimal digit and not 0x%x\n",str[0]);
      }
   return precision;
   }

int32_t
J9::DataType::getBCDStringFirstIndex(char *str, TR::DataType dt)
   {
   char firstChar = str[0];
   int32_t firstIndex = 0;
   if (TR::DataType::isBCDSignChar(firstChar))
      {
      firstIndex = TR::DataType::getBCDSignCharSize();
      }
   else
      {
      TR_ASSERT(isdigit(firstChar),"expecting a minus/plus sign or a decimal digit and not 0x%x\n",firstChar);
      firstIndex = 0;
      }
   return firstIndex;
   }

int32_t
J9::DataType::getBCDStringLastIndex(char *str, TR::DataType dt)
   {
   return strlen(str)-1;
   }

bool
J9::DataType::isValidEmbeddedSign(uint8_t sign)
   {
   if (sign >= TR::DataType::getFirstValidSignCode() && sign <= TR::DataType::getLastValidSignCode())
      return true;
   else
      return false;
   }

bool
J9::DataType::isValidPackedData(char *lit, int32_t start, int32_t end, bool isEvenPrecision)
   {
   if (start > end)
      return false;

   if (isEvenPrecision)
      {
      if ((lit[start]&0xF0) != 0)
         return false;
      }

   // check the low byte for sign and digit so the loop can just be concerned with the remaining digits
   uint8_t lowestByte = lit[end];
   uint8_t sign = (lowestByte&0x0F);
   uint8_t lowDigit = (lowestByte&0xF0) >> 4;
   if (!TR::DataType::isValidEmbeddedSign(sign))
      {
      return false;
      }
   if (lowDigit > 9)
      {
      return false;
      }

   for (int32_t i = start; i <= end-1; i++)
      {
      uint8_t data = lit[i];
      uint8_t hiDigit = (data & 0xF0) >> 4;
      uint8_t loDigit = data & 0x0F;
      if (hiDigit > 9 || loDigit > 9)  // digit has to be in range 0->0xf
         {
         return false;
         }
      }

   return true;
   }

bool
J9::DataType::exceedsPaddingThreshold(int32_t digits, TR::DataType dt)
   {
   TR_DigitSize digitSize = TR::DataType::getDigitSize(dt);
   switch (digitSize)
      {
      case HalfByteDigit:
         return digits > 2;
      case OneByteDigit:
         return digits > 1;
      case TwoByteDigit:
         return digits > 0;
      default:
         TR_ASSERT(false,"unknown digit size %d\n",digitSize);
      }
   return false;
   }

uint8_t
J9::DataType::encodedToPrintableDigit(int32_t digit)
   {
   TR_ASSERT(digit >=0 && digit <=9,"expecting a decimal digit not 0x%x\n",digit);
   digit&=0xf;
   // FIXME: is there a better way to do this conversion from a digit to a printable character?
   #if defined(TR_HOST_S390)
      return 0xF0|digit;   // EBCDIC
   #else
      return 0x30|digit;   // ASCII
   #endif
   }

int32_t J9::DataType::convertSignEncoding(TR::DataType sourceType, TR::DataType targetType, int32_t sourceEncoding)
   {
   if (sourceType == targetType)
      return sourceEncoding;

   TR_SignCodeSize sourceSize = TR::DataType::getSignCodeSize(sourceType);
   TR_SignCodeSize targetSize = TR::DataType::getSignCodeSize(targetType);

   TR_ASSERT(sourceSize != UnknownSignCodeSize,"could not find source sign code size for type %s\n",sourceType.toString());
   TR_ASSERT(targetSize != UnknownSignCodeSize,"could not find target sign code size for type %s\n",targetType.toString());

   if (sourceSize == targetSize)
      return sourceEncoding;

   int32_t sign = TR::DataType::getInvalidSignCode();
   switch (sourceSize)
      {
      case EmbeddedHalfByte:
         {
         if (TR::DataType::embeddedSignCodeIsInRange(sourceEncoding))
            {
            TR_ASSERT(targetSize == SeparateOneByte || targetSize == SeparateTwoByte,"target sign should be SeparateOneByte or SeparateTwoByte and not %d\n",targetSize);
            TR_BCDSignCode normalizedSignCode = decimalSignCodeMap[sourceEncoding];
            if (normalizedSignCode == bcd_plus || normalizedSignCode == bcd_unsigned)
               sign = (targetSize == SeparateOneByte) ? TR::DataType::getZonedSeparatePlus() : TR::DataType::getNationalSeparatePlus();
            else if (normalizedSignCode == bcd_minus)
               sign = (targetSize == SeparateOneByte) ? TR::DataType::getZonedSeparateMinus() : TR::DataType::getNationalSeparateMinus();
            else
               TR_ASSERT(false,"got bcd_sign_invalid for sign encoding 0x%x\n",sourceEncoding);
            }
         else
            {
            TR_ASSERT(false,"invalid source sign encoding of %d\n",sourceEncoding); // always return invalid when out of range so bug is reproducible
            }
         break;
         }
      case SeparateOneByte:
         {
         TR_ASSERT(targetSize == EmbeddedHalfByte || targetSize == SeparateTwoByte,"target sign should be EmbeddedHalfByte or SeparateTwoByte and not %d\n",targetSize);
         if (sourceEncoding == TR::DataType::getZonedSeparatePlus())
            sign = (targetSize == EmbeddedHalfByte) ? TR::DataType::getPreferredPlusCode() : TR::DataType::getNationalSeparatePlus();
         else if (sourceEncoding == TR::DataType::getZonedSeparateMinus())
            sign = (targetSize == EmbeddedHalfByte) ? TR::DataType::getPreferredMinusCode() : TR::DataType::getNationalSeparateMinus();
         else
            TR_ASSERT(false,"could not convert SeparateOneByte sign encoding 0x%x\n",sourceEncoding);
         break;
         }
      case SeparateTwoByte:
         {
         TR_ASSERT(targetSize == EmbeddedHalfByte || targetSize == SeparateOneByte,"target sign should be EmbeddedHalfByte or SeparateOneByte and not %d\n",targetSize);
         if (sourceEncoding == TR::DataType::getZonedSeparatePlus())
            sign = (targetSize == EmbeddedHalfByte) ? TR::DataType::getPreferredPlusCode() : TR::DataType::getNationalSeparatePlus();
         else if (sourceEncoding == TR::DataType::getZonedSeparateMinus())
            sign = (targetSize == EmbeddedHalfByte) ? TR::DataType::getPreferredMinusCode() : TR::DataType::getNationalSeparateMinus();
         else
            TR_ASSERT(false,"could not convert SeparateTwoByte sign encoding 0x%x\n",sourceEncoding);
         break;
         }
      default:
         TR_ASSERT(false,"unknown sourceSize of %d\n",sourceSize);
      }
   return sign;
   }

