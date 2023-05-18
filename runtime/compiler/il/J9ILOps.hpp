/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
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
   bool isFloatingPoint()                   const { return typeProperties().testAny(ILTypeProp::Floating_Point); }
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
   bool trackLineNo()                       const { return properties4().testAny(ILProp4::TrackLineNo); }
   bool canHavePaddingAddress()             const { return properties4().testAny(ILProp4::CanHavePaddingAddress); }
   bool canHaveStorageReferenceHint()       const { return properties4().testAny(ILProp4::CanHaveStorageReferenceHint); }
   bool canUseStoreAsAnAccumulator()        const { return properties4().testAny(ILProp4::CanUseStoreAsAnAccumulator); }


   bool isSignlessBCDType()          const {return (getDataType() == TR::UnicodeDecimal); }

   // i.e. a clean with no other side-effects
   bool isSimpleBCDClean() const {return _opCode == TR::pdclean; }

   bool isSqrt()
      {
      return OMR::ILOpCode::isSqrt();
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

   static TR::ILOpCodes getDataTypeConversion(TR::DataType t1, TR::DataType t2);

   static TR::ILOpCodes getProperConversion(TR::DataType sourceDataType, TR::DataType targetDataType, bool needUnsignedConversion)
      {
      TR::ILOpCodes op = getDataTypeConversion(sourceDataType, targetDataType);
      if (!needUnsignedConversion) return op;

      switch (op)
         {
         case TR::i2pd: return TR::iu2pd;
         case TR::l2pd: return TR::lu2pd;
         case TR::pd2i: return TR::pd2iu;
         case TR::pd2l: return TR::pd2lu;

         default: return OMR::ILOpCode::getProperConversion(sourceDataType, targetDataType, needUnsignedConversion);
         }
      }

   static bool isStrictlyLessThanCmp(TR::ILOpCodes op)
      {
            return OMR::ILOpCode::isStrictlyLessThanCmp(op);
      }

   static bool isStrictlyGreaterThanCmp(TR::ILOpCodes op)
      {
            return OMR::ILOpCode::isStrictlyGreaterThanCmp(op);
      }

   static bool isLessCmp(TR::ILOpCodes op)
      {
            return OMR::ILOpCode::isLessCmp(op);
      }

   static bool isGreaterCmp(TR::ILOpCodes op)
      {
            return OMR::ILOpCode::isGreaterCmp(op);
      }

   static bool isEqualCmp(TR::ILOpCodes op)
      {
            return OMR::ILOpCode::isEqualCmp(op);
      }

   static bool isNotEqualCmp(TR::ILOpCodes op)
      {
            return OMR::ILOpCode::isNotEqualCmp(op);
      }

   static TR::ILOpCodes cleanOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::PackedDecimal:      return TR::pdclean;
         default: TR_ASSERT(0, "no clean opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes absOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::PackedDecimal:      return TR::pdabs;
         default: return OMR::ILOpCode::absOpCode(type);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes addOpCode(TR::DataType type, bool is64Bit)
      {
      switch(type)
         {
         case TR::PackedDecimal:      return TR::pdadd;
         default: return OMR::ILOpCode::addOpCode(type, is64Bit);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes subtractOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::PackedDecimal:      return TR::pdsub;
         default: return OMR::ILOpCode::subtractOpCode(type);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes multiplyOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::PackedDecimal:     return TR::pdmul;
         default: return OMR::ILOpCode::multiplyOpCode(type);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes divideOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::PackedDecimal:     return TR::pddiv;
         default: return OMR::ILOpCode::divideOpCode(type);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes remainderOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::PackedDecimal:     return TR::pdrem;
         default: return OMR::ILOpCode::remainderOpCode(type);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes negateOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::PackedDecimal:     return TR::pdneg;
         default: return OMR::ILOpCode::negateOpCode(type);
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes modifyPrecisionOpCode(TR::DataType type)
      {
      switch (type)
         {
         case TR::PackedDecimal:     return TR::pdModifyPrecision;
         default: TR_ASSERT(0, "no modifyPrecision opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes shiftLeftOpCode(TR::DataType type)
      {
      return OMR::ILOpCode::shiftLeftOpCode(type);
      }

   static TR::ILOpCodes shiftRightOpCode(TR::DataType type)
      {
      return OMR::ILOpCode::shiftRightOpCode(type);
      }

   static TR::ILOpCodes ifcmpgeOpCode(TR::DataType type, bool isUnsigned)
      {
      return OMR::ILOpCode::ifcmpgeOpCode(type, isUnsigned);
      }

   static TR::ILOpCodes ifcmpleOpCode(TR::DataType type, bool isUnsigned)
      {
      return OMR::ILOpCode::ifcmpleOpCode(type, isUnsigned);
      }

   static TR::ILOpCodes ifcmpgtOpCode(TR::DataType type, bool isUnsigned)
      {
      return OMR::ILOpCode::ifcmpgtOpCode(type, isUnsigned);
      }

   static TR::ILOpCodes ifcmpltOpCode(TR::DataType type, bool isUnsigned)
      {
      return OMR::ILOpCode::ifcmpltOpCode(type, isUnsigned);
      }

   static TR::ILOpCodes ifcmpeqOpCode(TR::DataType type)
      {
      return OMR::ILOpCode::ifcmpeqOpCode(type);
      }

   static TR::ILOpCodes ifcmpneOpCode(TR::DataType type)
      {
      return OMR::ILOpCode::ifcmpneOpCode(type);
      }

   static TR::ILOpCodes cmpeqOpCode(TR::DataType type)
      {
      return OMR::ILOpCode::cmpeqOpCode(type);
      }

   static TR::ILOpCodes constOpCode(TR::DataType type)
      {
      return OMR::ILOpCode::constOpCode(type);
      }

   static TR::ILOpCodes returnOpCode(TR::DataType type)
      {
      return OMR::ILOpCode::returnOpCode(type);
      }

   };

}

#endif

