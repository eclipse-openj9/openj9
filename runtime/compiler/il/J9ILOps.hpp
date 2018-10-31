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

#ifndef J9ILOPS_INCL
#define J9ILOPS_INCL

#include "il/OMRILOps.hpp"

namespace J9 { 

class ILOpCode : public OMR::ILOpCode
   {

public:
 
   ILOpCode() : OMR::ILOpCode() {}
   ILOpCode(TR::ILOpCodes opCode) : OMR::ILOpCode(opCode) {}  

   /**
    * ILTypeProp, ILProp1, ILProp2, ILProp3
    */
   bool isDFP()                             const { return typeProperties().testAny(ILTypeProp::DFP); }
   bool isFloatingPoint()                   const { return typeProperties().testAny(ILTypeProp::Floating_Point | ILTypeProp::DFP); }
   bool isDFPRightShift()                   const { return isRightShift() && getType().isDFP(); }
   bool isDFPLeftShift()                    const { return isLeftShift() && getType().isDFP(); }
   bool isBCDLoadVar()                      const { return isLoadVar() && getType().isBCD(); }
   bool isBCDLoad()                         const { return isLoad() && getType().isBCD(); }
   bool isBCDStore()                        const { return isStore() && getType().isBCD(); }
   bool isPackedConst()                     const { return isLoadConst() && getType().isAnyPacked(); }
   bool isPackedStore()                     const { return isStore() && getType().isAnyPacked(); }
   bool isPackedAdd()                       const { return isAdd() && getType().isAnyPacked(); }
   bool isPackedSubtract()                  const { return isSub() && getType().isAnyPacked(); }
   bool isPackedMultiply()                  const { return isMul() && getType().isAnyPacked(); }
   bool isPackedDivide()                    const { return isDiv() && getType().isAnyPacked(); }
   bool isPackedRemainder()                 const { return isRem() && getType().isAnyPacked(); }
   bool isPackedShift()                     const { return isShift() && getType().isAnyPacked(); }
   bool isPackedRightShift()                const { return isRightShift() && getType().isAnyPacked(); }
   bool isPackedLeftShift()                 const { return isLeftShift() && getType().isAnyPacked(); }
   bool isBasicPackedArithmetic()           const { return (isAdd() || isSub() || isMul() || isDiv() || isRem()) && getType().isAnyPacked(); }

   /**
    * ILProp4
    */
   bool isSetSign()                         const { return properties4().testAny(ILProp4::SetSign); }
   bool isSetSignOnNode()                   const { return properties4().testAny(ILProp4::SetSignOnNode); }
   bool isModifyPrecision()                 const { return properties4().testAny(ILProp4::ModifyPrecision); }
   bool isPackedModifyPrecision()           const { return isModifyPrecision() && getType().isAnyPacked(); }
   bool isConversionWithFraction()          const { return properties4().testAny(ILProp4::ConversionHasFraction); }
   bool isBinaryCodedDecimalOp()            const { return properties4().testAny(ILProp4::BinaryCodedDecimalOp); }
   bool isAnyBCDCompareOp()                 const { return isBinaryCodedDecimalOp() && isBooleanCompare(); } // isAnyBCDCompareOp is true for all BCD nodes that do some type of compare e.g. pdcmpxx
   bool isBCDToNonBCDConversion()           const { return isConversion() && isBinaryCodedDecimalOp() && !getType().isBCD(); }
   bool isPackedArithmeticOverflowMessage() const { return properties4().testAny(ILProp4::PackedArithmeticOverflowMessage); }
   bool isBasicOrSpecialPackedArithmetic()  const { return isBasicPackedArithmetic() || isPackedArithmeticOverflowMessage(); }
   bool isAnyBCDArithmetic()                const { return isBasicOrSpecialPackedArithmetic(); }
   bool isDFPTestDataClass()                const { return properties4().testAny(ILProp4::DFPTestDataClass); }
   bool trackLineNo()                       const { return properties4().testAny(ILProp4::TrackLineNo); }
   bool canHavePaddingAddress()             const { return properties4().testAny(ILProp4::CanHavePaddingAddress); }
   bool canHaveStorageReferenceHint()       const { return properties4().testAny(ILProp4::CanHaveStorageReferenceHint); }
   bool canUseStoreAsAnAccumulator()        const { return properties4().testAny(ILProp4::CanUseStoreAsAnAccumulator); }


   bool isSignlessBCDType()          const {return (getDataType() == TR::UnicodeDecimal); }
   bool isZonedToDFPAbs() const
      {
      return getOpCodeValue() == TR::zd2dfAbs ||
             getOpCodeValue() == TR::zd2ddAbs ||
             getOpCodeValue() == TR::zd2deAbs;
      }

   bool isPackedToDFPAbs() const
      {
      return getOpCodeValue() == TR::pd2dfAbs ||
             getOpCodeValue() == TR::pd2ddAbs ||
             getOpCodeValue() == TR::pd2deAbs;
      }

   bool isZonedOrPackedToDFPAbs() const
      {
      return isZonedToDFPAbs() || isPackedToDFPAbs();
      }

   // i.e. a clean with no other side-effects
   bool isSimpleBCDClean() const {return _opCode == TR::pdclean; }

   bool isSqrt()
      {
      auto op = getOpCodeValue();
      if (op == TR::dfsqrt || op == TR::ddsqrt || op == TR::desqrt)
         return true;
      return OMR::ILOpCode::isSqrt();
      }

   static TR::ILOpCodes dfpToPackedCleanOp(TR::DataType dfpType, TR::DataType zonedType)
      {
      if (zonedType == TR::PackedDecimal)
         {
         switch(dfpType)
            {
            case TR::DecimalFloat:       return TR::df2pdClean;
            case TR::DecimalDouble:      return TR::dd2pdClean;
            case TR::DecimalLongDouble:  return TR::de2pdClean;
            default:                     return TR::BadILOp;
            }
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes dfpToZonedCleanOp(TR::DataType dfpType, TR::DataType zonedType)
      {
      if (zonedType == TR::ZonedDecimal)
         {
         switch(dfpType)
            {
            case TR::DecimalFloat:       return TR::df2zdClean;
            case TR::DecimalDouble:      return TR::dd2zdClean;
            case TR::DecimalLongDouble:  return TR::de2zdClean;
            default:                     return TR::BadILOp;
            }
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes dfpToZonedSetSignOp(TR::DataType dfpType, TR::DataType zonedType)
      {
      if (zonedType == TR::ZonedDecimal)
         {
         switch(dfpType)
            {
            case TR::DecimalFloat:       return TR::df2zdSetSign;
            case TR::DecimalDouble:      return TR::dd2zdSetSign;
            case TR::DecimalLongDouble:  return TR::de2zdSetSign;
            default:                     return TR::BadILOp;
            }
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes dfpToPackedSetSignOp(TR::DataType dfpType, TR::DataType packedType)
      {
      if (packedType == TR::PackedDecimal)
         {
         switch(dfpType)
            {
            case TR::DecimalFloat:       return TR::df2pdSetSign;
            case TR::DecimalDouble:      return TR::dd2pdSetSign;
            case TR::DecimalLongDouble:  return TR::de2pdSetSign;
            default:                     return TR::BadILOp;
            }
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes dfpToZonedOp(TR::DataType dfpType, TR::DataType zonedType)
      {
      if (zonedType == TR::ZonedDecimal)
         {
         switch(dfpType)
            {
            case TR::DecimalFloat:       return TR::df2zd;
            case TR::DecimalDouble:      return TR::dd2zd;
            case TR::DecimalLongDouble:  return TR::de2zd;
            default:                     return TR::BadILOp;
            }
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes packedToDFPAbsOp(TR::DataType zonedType, TR::DataType dfpType)
      {
      if (zonedType == TR::PackedDecimal)
         {
         switch(dfpType)
            {
            case TR::DecimalFloat:       return TR::pd2dfAbs;
            case TR::DecimalDouble:      return TR::pd2ddAbs;
            case TR::DecimalLongDouble:  return TR::pd2deAbs;
            default:                     return TR::BadILOp;
            }
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes zonedToDFPAbsOp(TR::DataType zonedType, TR::DataType dfpType)
      {
      if (zonedType == TR::ZonedDecimal)
         {
         switch(dfpType)
            {
            case TR::DecimalFloat:       return TR::zd2dfAbs;
            case TR::DecimalDouble:      return TR::zd2ddAbs;
            case TR::DecimalLongDouble:  return TR::zd2deAbs;
            default:                     return TR::BadILOp;
            }
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes setSignOpCode(TR::DataType type)
      {
      switch (type)
         {
         case TR::PackedDecimal:                    return TR::pdSetSign;
         default: TR_ASSERT(0, "no setSign opcode for this datatype");
         }
      return TR::BadILOp;
      }

   // returns the index (0->numChild-1) for the sign value to be set or retrieved
   static int32_t getSetSignValueIndex(TR::ILOpCodes op)
      {
      int32_t index = 0;
      switch (op)
         {
         case TR::pdSetSign:
         case TR::pd2zdslsSetSign:
         case TR::pd2zdstsSetSign:
            index = 1;  // child 2
            break;
         case TR::pdshlSetSign:
            index = 2;  // child 3
            break;
         case TR::pdshrSetSign:
            index = 3;  // child 4
            break;
         default:
            TR_ASSERT(false,"op is not a setSign operation\n");
         }
      return index;
      }

   static TR::ILOpCodes setSignVersionOfOpCode(TR::ILOpCodes op)
      {
      switch(op)
         {
         case TR::pdshr:       return TR::pdshrSetSign;
         case TR::pdshl:       return TR::pdshlSetSign;
         case TR::pd2zdsls:    return TR::pd2zdslsSetSign;
         case TR::pd2zdsts:    return TR::pd2zdstsSetSign;
         default: return TR::BadILOp;
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes reverseSetSignOpCode(TR::ILOpCodes op)
      {
      switch(op)
         {
         case TR::pdshrSetSign:        return TR::pdshr;
         case TR::pdshlSetSign:        return TR::pdshl;
         case TR::pd2zdslsSetSign:     return TR::pd2zdsls;
         case TR::pd2zdstsSetSign:     return TR::pd2zdsts;
         default: return TR::BadILOp;
         }
      return TR::BadILOp;
      }

   static bool isZonedToZonedConversion(TR::ILOpCodes op)
      {
      switch (op)
         {
         case TR::zd2zdsle:
         case TR::zdsle2zd:
            return true;
         default:
            return false;
         }
      }

   static bool isPackedConversionToWiderType(TR::ILOpCodes op)
      {
      switch(op)
         {
         case TR::pd2zd:
         case TR::pd2zdslsSetSign:
         case TR::pd2zdstsSetSign:
            return true;
         default:
            return false;
         }

      }

   static TR::ILOpCodes dfpOpForBCDOp(TR::ILOpCodes bcdOp, TR::DataType type)
      {
      TR_ASSERT(type == TR::DecimalFloat || type == TR::DecimalDouble || type == TR::DecimalLongDouble, "Invalid DFP type specified\n");

      switch(bcdOp)
         {
         case TR::pdadd:       return addOpCode(type, false);
         case TR::pdsub:       return subtractOpCode(type);
         case TR::pdmul:       return multiplyOpCode(type);
         case TR::pddiv:       return divideOpCode(type);
         case TR::pdneg:       return negateOpCode(type);
         case TR::pdshr:
            {
            switch(type)
               {
               case TR::DecimalFloat:      return TR::dfshr;
               case TR::DecimalDouble:     return TR::ddshr;
               case TR::DecimalLongDouble: return TR::deshr;
               default: return TR::BadILOp;
               }
            }
         case TR::pdshl:
            {
            switch(type)
               {
               case TR::DecimalFloat:      return TR::dfshl;
               case TR::DecimalDouble:     return TR::ddshl;
               case TR::DecimalLongDouble: return TR::deshl;
               default: return TR::BadILOp;
               }
            }
         case TR::pdcmpeq:
            {
            switch(type)
               {
               case TR::DecimalFloat:      return TR::dfcmpeq;
               case TR::DecimalDouble:     return TR::ddcmpeq;
               case TR::DecimalLongDouble: return TR::decmpeq;
               default: return TR::BadILOp;
               }
            }
         case TR::pdcmpne:
            {
            switch(type)
               {
               case TR::DecimalFloat:      return TR::dfcmpne;
               case TR::DecimalDouble:     return TR::ddcmpne;
               case TR::DecimalLongDouble: return TR::decmpne;
               default: return TR::BadILOp;
               }
            }
         case TR::pdcmplt:
            {
            switch(type)
               {
               case TR::DecimalFloat:      return TR::dfcmplt;
               case TR::DecimalDouble:     return TR::ddcmplt;
               case TR::DecimalLongDouble: return TR::decmplt;
               default: return TR::BadILOp;
               }
            }
         case TR::pdcmpge:
            {
            switch(type)
               {
               case TR::DecimalFloat:      return TR::dfcmpge;
               case TR::DecimalDouble:     return TR::ddcmpge;
               case TR::DecimalLongDouble: return TR::decmpge;
               default: return TR::BadILOp;
               }
            }
         case TR::pdcmpgt:
            {
            switch(type)
               {
               case TR::DecimalFloat:      return TR::dfcmpgt;
               case TR::DecimalDouble:     return TR::ddcmpgt;
               case TR::DecimalLongDouble: return TR::decmpgt;
               default: return TR::BadILOp;
               }
            }
         case TR::pdcmple:
            {
            switch(type)
               {
               case TR::DecimalFloat:      return TR::dfcmple;
               case TR::DecimalDouble:     return TR::ddcmple;
               case TR::DecimalLongDouble: return TR::decmple;
               default: return TR::BadILOp;
               }
            }
         default: TR_ASSERT(0,"unexpected binary coded decimal op\n");
                  return TR::BadILOp;
         }
      }

   static TR::ILOpCodes getProperConversion(TR::DataType sourceDataType, TR::DataType targetDataType, bool needUnsignedConversion)
      {
      TR::ILOpCodes op = TR::DataType::getDataTypeConversion(sourceDataType, targetDataType);
      if (!needUnsignedConversion) return op;

      switch (op)
         {
         case TR::b2df: return TR::bu2df;
         case TR::b2dd: return TR::bu2dd;
         case TR::b2de: return TR::bu2de;
         case TR::i2df: return TR::iu2df;
         case TR::i2dd: return TR::iu2dd;
         case TR::i2de: return TR::iu2de;
         case TR::l2df: return TR::lu2df;
         case TR::l2dd: return TR::lu2dd;
         case TR::l2de: return TR::lu2de;

         case TR::df2i: return TR::df2iu;
         case TR::df2l: return TR::df2lu;
         case TR::df2b: return TR::df2bu;
         case TR::df2s: return TR::df2s;

         case TR::dd2i: return TR::dd2iu;
         case TR::dd2l: return TR::dd2lu;
         case TR::dd2b: return TR::dd2bu;
         case TR::dd2s: return TR::dd2s;

         case TR::de2i: return TR::de2iu;
         case TR::de2l: return TR::de2lu;
         case TR::de2b: return TR::de2bu;
         case TR::de2s: return TR::de2s;
         case TR::i2pd: return TR::iu2pd;
         case TR::l2pd: return TR::lu2pd;
         case TR::pd2i: return TR::pd2iu;
         case TR::pd2l: return TR::pd2lu;

         default: return OMR::ILOpCode::getProperConversion(sourceDataType, targetDataType, needUnsignedConversion);
         }
      }

   static bool isStrictlyLessThanCmp(TR::ILOpCodes op)
      {
      switch (op)
         {
         case TR::ifdfcmplt:
         case TR::ifdfcmpltu:
         case TR::ifddcmplt:
         case TR::ifddcmpltu:
         case TR::ifdecmplt:
         case TR::ifdecmpltu:
            return true;
         default:
            return OMR::ILOpCode::isStrictlyLessThanCmp(op);
         }
      }

   static bool isStrictlyGreaterThanCmp(TR::ILOpCodes op)
      {
      switch (op)
         {
         case TR::ifdfcmpgt:
         case TR::ifdfcmpgtu:
         case TR::ifddcmpgt:
         case TR::ifddcmpgtu:
         case TR::ifdecmpgt:
         case TR::ifdecmpgtu:
            return true;
         default:
            return OMR::ILOpCode::isStrictlyGreaterThanCmp(op);
         }
      }

   static bool isLessCmp(TR::ILOpCodes op)
      {
      switch (op)
         {
         case TR::ifdfcmplt:
         case TR::ifdfcmpltu:
         case TR::ifdfcmple:
         case TR::ifdfcmpleu:
         case TR::ifddcmplt:
         case TR::ifddcmpltu:
         case TR::ifddcmple:
         case TR::ifddcmpleu:
         case TR::ifdecmplt:
         case TR::ifdecmpltu:
         case TR::ifdecmple:
         case TR::ifdecmpleu:
            return true;
         default:
            return OMR::ILOpCode::isLessCmp(op);
         }
      }

   static bool isGreaterCmp(TR::ILOpCodes op)
      {
      switch (op)
         {
         case TR::ifdfcmpgt:
         case TR::ifdfcmpgtu:
         case TR::ifddcmpgt:
         case TR::ifddcmpgtu:
         case TR::ifdecmpgt:
         case TR::ifdecmpgtu:

         case TR::ifdfcmpge:
         case TR::ifdfcmpgeu:
         case TR::ifddcmpge:
         case TR::ifddcmpgeu:
         case TR::ifdecmpge:
         case TR::ifdecmpgeu:
            return true;
         default:
            return OMR::ILOpCode::isGreaterCmp(op);

         }
      }

   static bool isEqualCmp(TR::ILOpCodes op)
      {
      switch (op)
         {
         case TR::ifdfcmpeq:
         case TR::ifddcmpeq:
         case TR::ifdecmpeq:
            return true;
         default:
            return OMR::ILOpCode::isEqualCmp(op);
         }
      }

   static bool isNotEqualCmp(TR::ILOpCodes op)
      {
      switch (op)
         {
         case TR::ifdfcmpne:
         case TR::ifddcmpne:
         case TR::ifdecmpne:
            return true;
         default:
            return OMR::ILOpCode::isNotEqualCmp(op);
         }
      }

   static TR::ILOpCodes cleanOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::DecimalDouble:      return TR::ddclean;
         case TR::DecimalLongDouble:  return TR::declean;
         case TR::PackedDecimal:      return TR::pdclean;
         default: TR_ASSERT(0, "no clean opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes absOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::DecimalFloat:       return TR::dfabs;
         case TR::DecimalDouble:      return TR::ddabs;
         case TR::DecimalLongDouble:  return TR::deabs;
         case TR::PackedDecimal:      return TR::pdabs;
         default: return OMR::ILOpCode::absOpCode(type);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes addOpCode(TR::DataType type, bool is64Bit)
      {
      switch(type)
         {
         case TR::DecimalFloat:       return TR::dfadd;
         case TR::DecimalDouble:      return TR::ddadd;
         case TR::DecimalLongDouble:  return TR::deadd;
         case TR::PackedDecimal:      return TR::pdadd;
         default: return OMR::ILOpCode::addOpCode(type, is64Bit);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes subtractOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::DecimalFloat:       return TR::dfsub;
         case TR::DecimalDouble:      return TR::ddsub;
         case TR::DecimalLongDouble:  return TR::desub;
         case TR::PackedDecimal:      return TR::pdsub;
         default: return OMR::ILOpCode::subtractOpCode(type);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes multiplyOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::DecimalFloat:      return TR::dfmul;
         case TR::DecimalDouble:     return TR::ddmul;
         case TR::DecimalLongDouble: return TR::demul;
         case TR::PackedDecimal:     return TR::pdmul;
         default: return OMR::ILOpCode::multiplyOpCode(type);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes divideOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::DecimalFloat:      return TR::dfdiv;
         case TR::DecimalDouble:     return TR::dddiv;
         case TR::DecimalLongDouble: return TR::dediv;
         case TR::PackedDecimal:     return TR::pddiv;
         default: return OMR::ILOpCode::divideOpCode(type);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes remainderOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::DecimalFloat:      return TR::dfrem;
         case TR::DecimalDouble:     return TR::ddrem;
         case TR::DecimalLongDouble: return TR::derem;
         case TR::PackedDecimal:     return TR::pdrem;
         default: return OMR::ILOpCode::remainderOpCode(type);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes negateOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::DecimalFloat:      return TR::dfneg;
         case TR::DecimalDouble:     return TR::ddneg;
         case TR::DecimalLongDouble: return TR::deneg;
         case TR::PackedDecimal:     return TR::pdneg;
         default: return OMR::ILOpCode::negateOpCode(type);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes insertExponentOp(TR::DataType type)
      {
      switch (type)
         {
         case TR::DecimalFloat:      return TR::dfInsExp;
         case TR::DecimalDouble:     return TR::ddInsExp;
         case TR::DecimalLongDouble: return TR::deInsExp;
         default: TR_ASSERT(0, "no insertExponent opcode for this dt %s",type.toString());
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes modifyPrecisionOpCode(TR::DataType type)
      {
      switch (type)
         {
         case TR::DecimalFloat:      return TR::dfModifyPrecision;
         case TR::DecimalDouble:     return TR::ddModifyPrecision;
         case TR::DecimalLongDouble: return TR::deModifyPrecision;
         case TR::PackedDecimal:     return TR::pdModifyPrecision;
         default: TR_ASSERT(0, "no modifyPrecision opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes setNegativeOpCode(TR::DataType type)
      {
      switch (type)
         {
         case TR::DecimalFloat:      return TR::dfSetNegative;
         case TR::DecimalDouble:     return TR::ddSetNegative;
         case TR::DecimalLongDouble: return TR::deSetNegative;
         default: TR_ASSERT(false, "no setNegative opcode for datatype %s",type.toString());
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes shiftLeftOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::DecimalFloat:      return TR::dfshl;
         case TR::DecimalDouble:     return TR::ddshl;
         case TR::DecimalLongDouble: return TR::deshl;
         default: return OMR::ILOpCode::shiftLeftOpCode(type);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes shiftRightOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::DecimalFloat:      return TR::dfshr;
         case TR::DecimalDouble:     return TR::ddshr;
         case TR::DecimalLongDouble: return TR::deshr;
         default: return OMR::ILOpCode::shiftRightOpCode(type);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes shiftRightRoundedOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::DecimalFloat:      return TR::dfshrRounded;
         case TR::DecimalDouble:     return TR::ddshrRounded;
         case TR::DecimalLongDouble: return TR::deshrRounded;
         default: TR_ASSERT(false, "no shrRounded opcode for datatype %d",type.getDataType());
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes ifcmpgeOpCode(TR::DataType type, bool isUnsigned)
      {
      switch(type)
         {
         case TR::DecimalFloat:       return TR::ifdfcmpge;
         case TR::DecimalDouble:      return TR::ifddcmpge;
         case TR::DecimalLongDouble:  return TR::ifdecmpge;
         default: return OMR::ILOpCode::ifcmpgeOpCode(type, isUnsigned);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes ifcmpleOpCode(TR::DataType type, bool isUnsigned)
      {
      switch(type)
         {
         case TR::DecimalFloat:       return TR::ifdfcmple;
         case TR::DecimalDouble:      return TR::ifddcmple;
         case TR::DecimalLongDouble:  return TR::ifdecmple;
         default: return OMR::ILOpCode::ifcmpleOpCode(type, isUnsigned);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes ifcmpgtOpCode(TR::DataType type, bool isUnsigned)
      {
      switch(type)
         {
         case TR::DecimalFloat:       return TR::ifdfcmpgt;
         case TR::DecimalDouble:      return TR::ifddcmpgt;
         case TR::DecimalLongDouble:  return TR::ifdecmpgt;
         default: return OMR::ILOpCode::ifcmpgtOpCode(type, isUnsigned);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes ifcmpltOpCode(TR::DataType type, bool isUnsigned)
      {
      switch(type)
         {
         case TR::DecimalFloat:       return TR::ifdfcmplt;
         case TR::DecimalDouble:      return TR::ifddcmplt;
         case TR::DecimalLongDouble:  return TR::ifdecmplt;
         default: return OMR::ILOpCode::ifcmpltOpCode(type, isUnsigned);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes ifcmpeqOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::DecimalFloat:       return TR::ifdfcmpeq;
         case TR::DecimalDouble:      return TR::ifddcmpeq;
         case TR::DecimalLongDouble:  return TR::ifdecmpeq;
         default: return OMR::ILOpCode::ifcmpeqOpCode(type);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes ifcmpneOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::DecimalFloat:       return TR::ifdfcmpne;
         case TR::DecimalDouble:      return TR::ifddcmpne;
         case TR::DecimalLongDouble:  return TR::ifdecmpne;
         default: return OMR::ILOpCode::ifcmpneOpCode(type);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes cmpeqOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::DecimalDouble:      return TR::ddcmpeq;
         case TR::DecimalLongDouble:  return TR::decmpeq;
         default: return OMR::ILOpCode::cmpeqOpCode(type);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes constOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::DecimalFloat:       return TR::dfconst;
         case TR::DecimalDouble:      return TR::ddconst;
         case TR::DecimalLongDouble:  return TR::deconst;
         default: return OMR::ILOpCode::constOpCode(type);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes returnOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::DecimalFloat:       return TR::dfreturn;
         case TR::DecimalDouble:      return TR::ddreturn;
         case TR::DecimalLongDouble:  return TR::dereturn;
         default: return OMR::ILOpCode::returnOpCode(type);
         }
      return TR::BadILOp;
      }

   };

}

#endif

