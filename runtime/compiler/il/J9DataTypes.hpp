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

#ifndef J9_DATATYPE_INCL
#define J9_DATATYPE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_DATATYPE_CONNECTOR
#define J9_DATATYPE_CONNECTOR
namespace J9 { class DataType; }
namespace J9 { typedef J9::DataType DataTypeConnector; }
#endif

#include "il/OMRDataTypes.hpp"

#include <stdint.h>

namespace TR { class DataType; }

// NOTE : update these tables when adding/modifying enum TR_BCDSignCode
// OMRDataTypes.hpp
//    bcdToRawSignCodeMap
//    rawToBCDSignCodeMap
//
// VPConstraint.hpp
//    bcdSignToSignConstraintMap
//    signConstraintToBCDSignMap
//
enum TR_BCDSignCode
   {
   bcd_invalid_sign   = 0,
   bcd_plus           = 1,
   bcd_minus          = 2,
   bcd_unsigned       = 3,
   num_bcd_sign_codes = 4
   };

#define TR_NUM_DECIMAL_CODES 16
const extern TR_BCDSignCode    decimalSignCodeMap[TR_NUM_DECIMAL_CODES];
const extern TR_RawBCDSignCode rawSignCodeMap[TR_NUM_DECIMAL_CODES];
const extern TR_RawBCDSignCode bcdToRawSignCodeMap[num_bcd_sign_codes];
const extern TR_BCDSignCode    rawToBCDSignCodeMap[num_raw_bcd_sign_codes];
const extern int32_t           packedDecimalPrecisionToByteLengthMap[TR_MAX_DECIMAL_PRECISION+1];

namespace J9
{

class OMR_EXTENSIBLE DataType : public OMR::DataTypeConnector
   {
public:
   DataType() : OMR::DataTypeConnector() { }
   DataType(TR::DataTypes t) : OMR::DataTypeConnector(t) { }

   static const char    * getName(TR::DataType dt);
   static const int32_t   getSize(TR::DataType dt);
   static void            setSize(TR::DataType dt, int32_t newValue);
   static const char    * getPrefix(TR::DataType dt);

   // for the overloaded instances of getName:
   // OMR::DataType::getName(TR_RawBCDSignCode)
   // OMR::DataType::getName(TR_BCDSignCode)
   using OMR::DataTypeConnector::getName;

   inline bool isFloatingPoint();
   bool isLongDouble()              { return _type == TR::DecimalLongDouble; }
   bool isDFP()                     { return _type == TR::DecimalFloat|| _type == TR::DecimalDouble|| _type == TR::DecimalLongDouble; }
   bool isBCD()                     { return (_type >= TR::FirstBCDType) && (_type <= TR::LastBCDType); }
   bool isAnyPacked()               { return _type == TR::PackedDecimal; }
   bool isAnyZoned()                { return (_type >= TR::FirstZonedType) && (_type <= TR::LastZonedType); }
   bool isAnyUnicode()              { return (_type >= TR::FirstUnicodeType) && (_type <= TR::LastUnicodeType); }
   inline bool isEmbeddedSign();
   bool isZonedEmbeddedSign()       { return (_type == TR::ZonedDecimal) || (_type == TR::ZonedDecimalSignLeadingEmbedded); }
   bool isZonedLeadingSign()        { return (_type == TR::ZonedDecimalSignLeadingSeparate) || (_type == TR::ZonedDecimalSignLeadingEmbedded); }
   bool isZonedTrailingSign()       { return (_type == TR::ZonedDecimalSignTrailingSeparate) || (_type == TR::ZonedDecimal); }
   bool isZonedSeparateSign()       { return (_type == TR::ZonedDecimalSignLeadingSeparate) || (_type == TR::ZonedDecimalSignTrailingSeparate); }
   bool isZonedTrailingSepSign()    { return _type == TR::ZonedDecimalSignTrailingSeparate; }
   bool isZonedLeadingSepSign()     { return _type == TR::ZonedDecimalSignLeadingSeparate; }
   bool isUnicodeTrailingSign()     { return _type == TR::UnicodeDecimalSignTrailing; }
   bool isUnicodeLeadingSign()      { return _type == TR::UnicodeDecimalSignLeading; }
   bool isUnicodeSeparateSign()     { return (_type == TR::UnicodeDecimalSignLeading) || (_type == TR::UnicodeDecimalSignTrailing); }
   inline bool isUnicodeSignedType();
   inline bool isSeparateSignType();
   inline bool isLeadingSignType();
   inline bool isSeparateSign();
   inline bool isLeadingSeparateSign();
   inline bool isTrailingSeparateSign();
   inline bool isLeadingSign();
   inline bool isTrailingSign();
   inline bool isSignless();
   inline bool hasExposedConstantAddress();

   static inline int32_t getMaxShortDFPPrecision()    { return 7; }
   static inline int32_t getMaxLongDFPPrecision()     { return 16; }
   static inline int32_t getMaxExtendedDFPPrecision() { return 34; }

   // NOTE: getMaxPackedDecimalSize is derived from TR_MAX_DECIMAL_PRECISION but left as a constant
   //       to aid in static char declarations see packedDecimalPrecisionToByteLength for the formula
   // an assume in the TR::Compilation constructor will fire if there is a mismatch
   static inline int32_t getMaxPackedDecimalSize()       { return 32; }
   static inline int32_t getMaxPackedDecimalPrecision()  { return 31; }
   static inline int32_t getInvalidDecimalPrecision()    { return 0; }

   static inline int32_t getMaxDecimalAdjust()   { return 63; }
   static inline int32_t getMinDecimalAdjust()   { return -63; }
   static inline int32_t getMaxDecimalRound()    { return 9; }
   static inline int32_t getMaxDecimalFraction() { return J9::DataType::getMaxDecimalAdjust(); } // fraction and adjust share the variable
   static inline int32_t getMinDecimalFraction() { return J9::DataType::getMinDecimalAdjust(); } // fraction and adjust share the variable
   static inline int32_t getMaxSrcIntDigits()    { return 63; }

   static inline int32_t getFirstValidSignCode() { return 0x0A; }
   static inline int32_t getLastValidSignCode()  { return 0x0F; }
   static inline int32_t getPreferredPlusCode()  { return 0x0C; }
   static inline int32_t getAlternatePlusCode()  { return 0x0A; }
   static inline int32_t getPreferredMinusCode() { return 0x0D; }
   static inline int32_t getAlternateMinusCode() { return 0x0B; }
   static inline int32_t getUnsignedCode()       { return 0x0F; }
   static inline int32_t getInvalidSignCode()    { return 0x00; } // reserve zero to be the invalid sign marker
   static inline int32_t getIgnoredSignCode()    { return -1; } // a special marker value for when a setSign operation can set any or no sign code value (as it will be ignored by consumer)

   static inline int32_t getZonedCode()             { return 0xF0; }
   static inline int32_t getZonedValue()            { return (J9::DataType::getZonedCode() >> 4) & 0x0F; }
   static inline int32_t getZonedZeroCode()         { return 0xF0; }
   static inline int32_t getZonedSeparatePlus()     { return 0x4E; }
   static inline int32_t getZonedSeparateMinus()    { return 0x60; }
   static inline int32_t getNationalSeparatePlus()  { return 0x2B; }
   static inline int32_t getNationalSeparateMinus() { return 0x2D; }

   static inline int32_t getUnicodeZeroCodeHigh()  { return 0x00; }
   static inline int32_t getUnicodeZeroCodeLow()   { return 0x30; }
   static inline int32_t getUnicodeZeroCode()      { return ((J9::DataType::getUnicodeZeroCodeHigh() << 8) | J9::DataType::getUnicodeZeroCodeLow()) & 0xFFFF; }
   static inline int32_t getUnicodePlusCodeHigh()  { return 0x00; }
   static inline int32_t getUnicodePlusCodeLow()   { return 0x2B; }
   static inline int32_t getUnicodePlusCode()      { return ((J9::DataType::getUnicodePlusCodeHigh() << 8) | J9::DataType::getUnicodePlusCodeLow()) & 0xFFFF; }
   static inline int32_t getUnicodeMinusCodeHigh() { return 0x00; }
   static inline int32_t getUnicodeMinusCodeLow()  { return 0x2D; }
   static inline int32_t getUnicodeMinusCode()     { return ((J9::DataType::getUnicodeMinusCodeHigh() << 8) | J9::DataType::getUnicodeMinusCodeLow()) & 0xFFFF; }

   static inline char getBCDPlusChar()     { return '+'; }
   static inline char getBCDMinusChar()    { return '-'; }
   static inline char getBCDUnsignedChar() { return 'u'; }

   static inline int32_t getZonedFloatSpace() { return 0x40; }  // EBCDIC ' '
   static inline int32_t getZonedFloatExpE()  { return 0xC5; }  // EBCDIC 'E'
   static inline int32_t getZonedFloatDot()   { return 0x4B; }  // EBCDIC '.'
   static inline int32_t getZonedFloatComma() { return 0x6B; }  // EBCDIC ','
   static inline int32_t getZonedFloatPlus()  { return 0x4E; }  // EBCDIC '+'
   static inline int32_t getZonedFloatMinus() { return 0x60; }  // EBCDIC '-'

   // size of the sign code in bytes (or rounded up to the next byte as in the embedded zoned/packed case)
   static inline int32_t getEmbeddedSignSize()   { return 1; }
   static inline int32_t getPackedSignSize()     { return 1; }
   static inline int32_t getZonedSignSize()      { return 1; }
   static inline int32_t getZonedElementSize()   { return 1; }
   static inline int32_t getUnicodeSignSize()    { return 2; }
   static inline int32_t getUnicodeElementSize() { return 2; }

   static inline int32_t getBCDSignCharSize() { return 1; } // one byte for leading leading +/-/u char
   static inline int32_t getBCDStrlen()       { return TR_MAX_DECIMAL_PRECISION + J9::DataType::getBCDSignCharSize() + 1; }
                                                // +1 for sign, +1 for terminating null -- for pretty printed string e.g. "+1234"

   static bool isValidZonedDigit(uint8_t data);
   static bool isValidUnicodeDigit(uint8_t dataHi, uint8_t dataLo);
   static bool isValidZonedSeparateSign(uint8_t data);
   static bool isValidUnicodeSeparateSign(uint8_t signHi, uint8_t signLo);
   static bool isValidZonedData(char  *lit, int32_t start, int32_t end);
   static bool isValidUnicodeData(char  *lit, int32_t start, int32_t end);
   static bool isValidBCDLiteral(char *lit, size_t litSize, TR::DataType dt, bool isEvenPrecision);

   static TR::DataType getDFPTypeFromPrecision(int32_t precision);

   bool canGetMaxPrecisionFromType();
   int32_t getMaxPrecisionFromType();

   int32_t separateSignSize();

   static int32_t getLeftMostByte(TR::DataType dt, int32_t dataSize);

   static int32_t getSignCodeOffset(TR::DataType dt, int32_t size);
   static TR_SignCodeSize getSignCodeSize(TR::DataType dt);
   static TR_BCDSignCode getNormalizedSignCode(TR::DataType dt, int32_t rawSignCode);

   static int32_t getSizeFromBCDPrecision(TR::DataType dt, int32_t precision);
   static int32_t getBCDPrecisionFromSize(TR::DataType dt, int32_t size);

   static uint32_t printableToEncodedSign(uint32_t printableSign, TR::DataType dt);
   static uint32_t encodedToPrintableSign(uint32_t encodedSign, TR::DataType dt);

   static uint8_t getOneByteBCDFill(TR::DataType dt);
   static uint16_t getTwoByteBCDFill(TR::DataType dt);

   static TR_DigitSize getDigitSize(TR::DataType dt);
   static int32_t bytesToDigits(TR::DataType dt, int32_t bytes);
   static int32_t digitsToBytes(TR::DataType dt, int32_t digits);

   static int32_t getPreferredPlusSignCode(TR::DataType dt);
   static int32_t getPreferredMinusSignCode(TR::DataType dt);

   static bool isValidEmbeddedSign(uint8_t sign);
   static bool isValidPackedData(char *lit, int32_t start, int32_t end, bool isEvenPrecision);

   static bool isBCDSignChar(char sign)
      {
      if (sign == J9::DataType::getBCDPlusChar() ||
          sign == J9::DataType::getBCDMinusChar() ||
          sign == J9::DataType::getBCDUnsignedChar())
         return true;
      else
         return false;
      }

   static TR_BCDSignCode getBCDSignCode(char *str)
      {
      if (str)
         {
         if (str[0] == J9::DataType::getBCDUnsignedChar())
            return bcd_unsigned;
         else if (str[0] == J9::DataType::getBCDPlusChar())
            return bcd_plus;
         else if (str[0] == J9::DataType::getBCDMinusChar())
            return bcd_minus;
         else
            return bcd_invalid_sign;
         }
      return bcd_invalid_sign;
      }

   static char *getName(TR_RawBCDSignCode s)
      {
      if (s < num_raw_bcd_sign_codes)
         return _TR_RawBCDSignCodeNames[s];
      else
         return (char*)"unknown raw sign";
      }

   static char *getName(TR_BCDSignCode s)
      {
      if (s < num_bcd_sign_codes)
         return _TR_BCDSignCodeNames[s];
      else
         return (char*)"unknown bcd sign";
      }

   static int32_t getValue(TR_RawBCDSignCode s)
      {
      if (s < num_raw_bcd_sign_codes)
         return _TR_RawBCDSignCodeValues[s];
      else
         return J9::DataType::getInvalidSignCode();
      }

   inline static bool isSupportedRawSign(int32_t sign);

   static TR_RawBCDSignCode getSupportedRawSign(int32_t sign)
      {
      TR_RawBCDSignCode rawSign = raw_bcd_sign_unknown;
      if (sign >=0 && sign < TR_NUM_DECIMAL_CODES)
         rawSign = rawSignCodeMap[sign];
      return rawSign;
      }

   static TR_RawBCDSignCode getRawSignFromBCDSign(TR_BCDSignCode bcdSign)
      {
      TR_RawBCDSignCode rawSign = raw_bcd_sign_unknown;
      if (bcdSign >=0 && bcdSign < num_bcd_sign_codes)
         rawSign = bcdToRawSignCodeMap[bcdSign];
      return rawSign;
      }

   static TR_BCDSignCode getBCDSignFromRawSign(TR_RawBCDSignCode rawSign)
      {
      TR_BCDSignCode bcdSign = bcd_invalid_sign;
      if (rawSign >=0 && rawSign < num_raw_bcd_sign_codes)
         bcdSign = rawToBCDSignCodeMap[rawSign];
      return bcdSign;
      }


   static int32_t packedDecimalPrecisionToByteLength(int32_t precision)
      {
      int32_t byteLength = 0;
      if (precision >= 0 && precision <= TR_MAX_DECIMAL_PRECISION)
         {
         byteLength = packedDecimalPrecisionToByteLengthMap[precision];
         }
      else
         {
         int32_t byteLength = precision + 1;
         if (byteLength & 0x1)
            byteLength = (byteLength/2) + 1;
         else
            byteLength = (byteLength/2);
         }
      return byteLength;
      }

   static int32_t byteLengthToPackedDecimalPrecisionCeiling(int32_t byteLength)
      {
      return (byteLength*2) - 1;
      }

   static int32_t byteLengthToPackedDecimalPrecisionFloor(int32_t byteLength)
      {
      return (byteLength*2) - 2;
      }

   static int32_t numBCDTypes() { return (TR::LastBCDType-TR::FirstBCDType) + 1; }

   static uint8_t encodedToPrintableDigit(int32_t digit);
   static bool embeddedSignCodeIsInRange(int32_t sign) { return (sign >=0 && sign < TR_NUM_DECIMAL_CODES); }

   static int32_t getBCDStringFirstIndex(char *str, TR::DataType dt);
   static int32_t getBCDStringLastIndex(char *str, TR::DataType dt);

   static int32_t convertSignEncoding(TR::DataType sourceType, TR::DataType targetType, int32_t sourceEncoding);

   static int32_t getBCDPrecisionFromString(char *str, TR::DataType dt);
   static bool exceedsPaddingThreshold(int32_t digits, TR::DataType dt);
   static bool rawSignIsPositive(TR::DataType dt, int32_t rawSignCode);
   static bool normalizedSignIsPositive(TR::DataType dt, TR_BCDSignCode normalizedSign);
   static bool rawSignIsNegative(TR::DataType dt, int32_t rawSignCode);
   static bool normalizedSignIsNegative(TR::DataType dt, TR_BCDSignCode normalizedSign);

   static TR::ILOpCodes getDataTypeConversion(TR::DataType t1, TR::DataType t2);

private:
   static char*         _TR_RawBCDSignCodeNames[num_raw_bcd_sign_codes];
   static int32_t       _TR_RawBCDSignCodeValues[num_raw_bcd_sign_codes];
   static char*         _TR_BCDSignCodeNames[num_bcd_sign_codes];
   };

}

#endif

