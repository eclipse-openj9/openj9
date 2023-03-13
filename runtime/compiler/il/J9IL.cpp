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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "il/IL.hpp"
#include "il/ILOpCodes.hpp"
#include "il/OMRILOps.hpp"
#include "il/DataTypes.hpp"
#include "infra/Assert.hpp"


TR::ILOpCodes J9::IL::opCodesForConst[] =
   {
   TR::BadILOp,    // TR::PackedDecimal
   TR::BadILOp,    // TR::ZonedDecimal
   TR::BadILOp,    // TR::ZonedDecimalSignLeadingEmbedded
   TR::BadILOp,    // TR::ZonedDecimalSignLeadingSeparate
   TR::BadILOp,    // TR::ZonedDecimalSignTrailingSeparate
   TR::BadILOp,    // TR::UnicodeDecimal
   TR::BadILOp,    // TR::UnicodeDecimalSignLeading
   TR::BadILOp     // TR::UnicodeDecimalSignTrailing
   };

TR::ILOpCodes J9::IL::opCodesForDirectLoad[] =
   {
   TR::pdload,     // TR::PackedDecimal
   TR::zdload,     // TR::ZonedDecimal
   TR::zdsleLoad,  // TR::ZonedDecimalSignLeadingEmbedded
   TR::zdslsLoad,  // TR::ZonedDecimalSignLeadingSeparate
   TR::zdstsLoad,  // TR::ZonedDecimalSignTrailingSeparate
   TR::udLoad,     // TR::UnicodeDecimal
   TR::udslLoad,   // TR::UnicodeDecimalSignLeading
   TR::udstLoad    // TR::UnicodeDecimalSignTrailing
   };

TR::ILOpCodes J9::IL::opCodesForDirectStore[] =
   {
   TR::pdstore,    // TR::PackedDecimal
   TR::zdstore,    // TR::ZonedDecimal
   TR::zdsleStore, // TR::ZonedDecimalSignLeadingEmbedded
   TR::zdslsStore, // TR::ZonedDecimalSignLeadingSeparate
   TR::zdstsStore, // TR::ZonedDecimalSignTrailingSeparate
   TR::udStore,    // TR::UnicodeDecimal
   TR::udslStore,  // TR::UnicodeDecimalSignLeading
   TR::udstStore   // TR::UnicodeDecimalSignTrailing
   };

TR::ILOpCodes J9::IL::opCodesForIndirectLoad[] =
   {
   TR::pdloadi,    // TR::PackedDecimal
   TR::zdloadi,    // TR::ZonedDecimal
   TR::zdsleLoadi, // TR::ZonedDecimalSignLeadingEmbedded
   TR::zdslsLoadi, // TR::ZonedDecimalSignLeadingSeparate
   TR::zdstsLoadi, // TR::ZonedDecimalSignTrailingSeparate
   TR::udLoadi,    // TR::UnicodeDecimal
   TR::udslLoadi,  // TR::UnicodeDecimalSignLeading
   TR::udstLoadi   // TR::UnicodeDecimalSignTrailing
   };

TR::ILOpCodes J9::IL::opCodesForIndirectStore[] =
   {
   TR::pdstorei,    // TR::PackedDecimal
   TR::zdstorei,    // TR::ZonedDecimal
   TR::zdsleStorei, // TR::ZonedDecimalSignLeadingEmbedded
   TR::zdslsStorei, // TR::ZonedDecimalSignLeadingSeparate
   TR::zdstsStorei, // TR::ZonedDecimalSignTrailingSeparate
   TR::udStorei,    // TR::UnicodeDecimal
   TR::udslStorei,  // TR::UnicodeDecimalSignLeading
   TR::udstStorei   // TR::UnicodeDecimalSignTrailing
   };

TR::ILOpCodes J9::IL::opCodesForIndirectArrayLoad[] =
   {
   TR::pdloadi,    // TR::PackedDecimal
   TR::zdloadi,    // TR::ZonedDecimal
   TR::zdsleLoadi, // TR::ZonedDecimalSignLeadingEmbedded
   TR::zdslsLoadi, // TR::ZonedDecimalSignLeadingSeparate
   TR::zdstsLoadi, // TR::ZonedDecimalSignTrailingSeparate
   TR::udLoadi,    // TR::UnicodeDecimal
   TR::udslLoadi,  // TR::UnicodeDecimalSignLeading
   TR::udstLoadi   // TR::UnicodeDecimalSignTrailing
   };

TR::ILOpCodes J9::IL::opCodesForIndirectArrayStore[] =
   {
   TR::pdstorei,    // TR::PackedDecimal
   TR::zdstorei,    // TR::ZonedDecimal
   TR::zdsleStorei, // TR::ZonedDecimalSignLeadingEmbedded
   TR::zdslsStorei, // TR::ZonedDecimalSignLeadingSeparate
   TR::zdstsStorei, // TR::ZonedDecimalSignTrailingSeparate
   TR::udStorei,    // TR::UnicodeDecimal
   TR::udslStorei,  // TR::UnicodeDecimalSignLeading
   TR::udstStorei   // TR::UnicodeDecimalSignTrailing
   };

TR::ILOpCodes J9::IL::opCodesForRegisterLoad[] =
   {
   TR::BadILOp,    // TR::PackedDecimal
   TR::BadILOp,    // TR::ZonedDecimal
   TR::BadILOp,    // TR::ZonedDecimalSignLeadingEmbedded
   TR::BadILOp,    // TR::ZonedDecimalSignLeadingSeparate
   TR::BadILOp,    // TR::ZonedDecimalSignTrailingSeparate
   TR::BadILOp,    // TR::UnicodeDecimal
   TR::BadILOp,    // TR::UnicodeDecimalSignLeading
   TR::BadILOp     // TR::UnicodeDecimalSignTrailing
   };

TR::ILOpCodes J9::IL::opCodesForRegisterStore[] =
   {
   TR::BadILOp,    // TR::PackedDecimal
   TR::BadILOp,    // TR::ZonedDecimal
   TR::BadILOp,    // TR::ZonedDecimalSignLeadingEmbedded
   TR::BadILOp,    // TR::ZonedDecimalSignLeadingSeparate
   TR::BadILOp,    // TR::ZonedDecimalSignTrailingSeparate
   TR::BadILOp,    // TR::UnicodeDecimal
   TR::BadILOp,    // TR::UnicodeDecimalSignLeading
   TR::BadILOp     // TR::UnicodeDecimalSignTrailing
   };

TR::ILOpCodes J9::IL::opCodesForCompareEquals[] =
   {
   TR::pdcmpeq,    // TR::PackedDecimal
   TR::BadILOp,    // TR::ZonedDecimal
   TR::BadILOp,    // TR::ZonedDecimalSignLeadingEmbedded
   TR::BadILOp,    // TR::ZonedDecimalSignLeadingSeparate
   TR::BadILOp,    // TR::ZonedDecimalSignTrailingSeparate
   TR::BadILOp,    // TR::UnicodeDecimal
   TR::BadILOp,    // TR::UnicodeDecimalSignLeading
   TR::BadILOp     // TR::UnicodeDecimalSignTrailing
   };

TR::ILOpCodes J9::IL::opCodesForCompareNotEquals[] =
   {
   TR::pdcmpne,    // TR::PackedDecimal
   TR::BadILOp,    // TR::ZonedDecimal
   TR::BadILOp,    // TR::ZonedDecimalSignLeadingEmbedded
   TR::BadILOp,    // TR::ZonedDecimalSignLeadingSeparate
   TR::BadILOp,    // TR::ZonedDecimalSignTrailingSeparate
   TR::BadILOp,    // TR::UnicodeDecimal
   TR::BadILOp,    // TR::UnicodeDecimalSignLeading
   TR::BadILOp     // TR::UnicodeDecimalSignTrailing
   };

static_assert(TR::LastJ9Type - TR::FirstJ9Type + 1 == (sizeof(J9::IL::opCodesForConst) / sizeof(J9::IL::opCodesForConst[0])),
              "J9::IL::opCodesForConst is not the correct size");

static_assert(TR::LastJ9Type - TR::FirstJ9Type + 1 == (sizeof(J9::IL::opCodesForDirectLoad) / sizeof(J9::IL::opCodesForDirectLoad[0])),
              "J9::IL::opCodesForDirectLoad is not the correct size");

static_assert(TR::LastJ9Type - TR::FirstJ9Type + 1 == (sizeof(J9::IL::opCodesForDirectStore) / sizeof(J9::IL::opCodesForDirectStore[0])),
              "J9::IL::opCodesForDirectStore is not the correct size");

static_assert(TR::LastJ9Type - TR::FirstJ9Type + 1 == (sizeof(J9::IL::opCodesForIndirectLoad) / sizeof(J9::IL::opCodesForIndirectLoad[0])),
              "J9::IL::opCodesForIndirectLoad is not the correct size");

static_assert(TR::LastJ9Type - TR::FirstJ9Type + 1 == (sizeof(J9::IL::opCodesForIndirectStore) / sizeof(J9::IL::opCodesForIndirectStore[0])),
              "J9::IL::opCodesForIndirectStore is not the correct size");

static_assert(TR::LastJ9Type - TR::FirstJ9Type + 1 == (sizeof(J9::IL::opCodesForIndirectArrayLoad) / sizeof(J9::IL::opCodesForIndirectArrayLoad[0])),
              "J9::IL::opCodesForIndirectArrayLoad is not the correct size");

static_assert(TR::LastJ9Type - TR::FirstJ9Type + 1 == (sizeof(J9::IL::opCodesForIndirectArrayStore) / sizeof(J9::IL::opCodesForIndirectArrayStore[0])),
              "J9::IL::opCodesForIndirectArrayStore is not the correct size");

static_assert(TR::LastJ9Type - TR::FirstJ9Type + 1 == (sizeof(J9::IL::opCodesForRegisterLoad) / sizeof(J9::IL::opCodesForRegisterLoad[0])),
              "J9::IL::opCodesForRegisterLoad is not the correct size");

static_assert(TR::LastJ9Type - TR::FirstJ9Type + 1 == (sizeof(J9::IL::opCodesForRegisterStore) / sizeof(J9::IL::opCodesForRegisterStore[0])),
              "J9::IL::opCodesForRegisterStore is not the correct size");

static_assert(TR::LastJ9Type - TR::FirstJ9Type + 1 == (sizeof(J9::IL::opCodesForCompareEquals) / sizeof(J9::IL::opCodesForCompareEquals[0])),
              "J9::IL::opCodesForCompareEquals is not the correct size");

static_assert(TR::LastJ9Type - TR::FirstJ9Type + 1 == (sizeof(J9::IL::opCodesForCompareNotEquals) / sizeof(J9::IL::opCodesForCompareNotEquals[0])),
              "J9::IL::opCodesForCompareNotEquals is not the correct size");


TR::ILOpCodes
J9::IL::opCodeForCorrespondingIndirectLoad(TR::ILOpCodes loadOpCode)
   {
   if (OMR::ILOpCode::isOMROpCode(loadOpCode))
      {
      return OMR::IL::opCodeForCorrespondingIndirectLoad(loadOpCode);
      }

   switch (loadOpCode)
      {
      case TR::pdloadi: return TR::pdstorei;
      case TR::zdloadi: return TR::zdstorei;
      case TR::zdsleLoadi: return TR::zdsleStorei;
      case TR::zdslsLoadi: return TR::zdslsStorei;
      case TR::zdstsLoadi: return TR::zdstsStorei;
      case TR::udLoadi: return TR::udStorei;
      case TR::udslLoadi: return TR::udslStorei;
      case TR::udstLoadi: return TR::udstStorei;
      default: return OMR::IL::opCodeForCorrespondingIndirectLoad(loadOpCode);
      }

   TR_ASSERT(0, "no corresponding store opcode for specified load opcode");
   return TR::BadILOp;
   }


TR::ILOpCodes
J9::IL::opCodeForCorrespondingIndirectStore(TR::ILOpCodes storeOpCode)
   {

   if (OMR::ILOpCode::isOMROpCode(storeOpCode))
      {
      return OMR::IL::opCodeForCorrespondingIndirectStore(storeOpCode);
      }

   switch (storeOpCode)
      {
      case TR::pdstorei: return TR::pdloadi;
      case TR::zdstorei: return TR::zdloadi;
      case TR::zdsleStorei: return TR::zdsleLoadi;
      case TR::zdslsStorei: return TR::zdslsLoadi;
      case TR::zdstsStorei: return TR::zdstsLoadi;
      case TR::udStorei: return TR::udLoadi;
      case TR::udslStorei: return TR::udslLoadi;
      case TR::udstStorei: return TR::udstLoadi;
      default: return OMR::IL::opCodeForCorrespondingIndirectStore(storeOpCode);
      }

   TR_ASSERT(0, "no corresponding load opcode for specified store opcode");
   return TR::BadILOp;
   }

TR::ILOpCodes
J9::IL::opCodeForCorrespondingDirectLoad(TR::ILOpCodes loadOpCode)
   {

   if (OMR::ILOpCode::isOMROpCode(loadOpCode))
      {
      return OMR::IL::opCodeForCorrespondingDirectLoad(loadOpCode);
      }

   switch (loadOpCode)
      {
      case TR::pdload: return TR::pdstore;
      case TR::zdload: return TR::zdstore;
      case TR::zdsleLoad: return TR::zdsleStore;
      case TR::zdslsLoad: return TR::zdslsStore;
      case TR::zdstsLoad: return TR::zdstsStore;
      case TR::udLoad: return TR::udStore;
      case TR::udslLoad: return TR::udslStore;
      case TR::udstLoad: return TR::udstStore;
      default: return OMR::IL::opCodeForCorrespondingDirectLoad(loadOpCode);
      }

   TR_ASSERT(0, "no corresponding store opcode for specified load opcode");
   return TR::BadILOp;
   }


TR::ILOpCodes
J9::IL::opCodeForCorrespondingDirectStore(TR::ILOpCodes storeOpCode)
   {

   if (OMR::ILOpCode::isOMROpCode(storeOpCode))
      {
      return OMR::IL::opCodeForCorrespondingDirectStore(storeOpCode);
      }

   switch (storeOpCode)
      {
      case TR::pdstore: return TR::pdload;
      case TR::zdstore: return TR::zdload;
      case TR::zdsleStore: return TR::zdsleLoad;
      case TR::zdslsStore: return TR::zdslsLoad;
      case TR::zdstsStore: return TR::zdstsLoad;
      case TR::udStore: return TR::udLoad;
      case TR::udslStore: return TR::udslLoad;
      case TR::udstStore: return TR::udstLoad;
      default: return OMR::IL::opCodeForCorrespondingDirectStore(storeOpCode);
      }

   TR_ASSERT(0, "no corresponding load opcode for specified store opcode");
   return TR::BadILOp;
   }

TR::ILOpCodes
J9::IL::opCodeForConst(TR::DataType dt)
   {
   if (dt == TR::Int8 || dt == TR::Int16)
      {
      return TR::iconst;
      }

   if (dt.isOMRDataType())
      {
      return OMR::IL::opCodeForConst(dt);
      }

   return J9::IL::opCodesForConst[dt - TR::FirstJ9Type];
   }


TR::ILOpCodes
J9::IL::opCodeForDirectReadBarrier(TR::DataType dt)
   {
   if (dt == TR::Int8 || dt == TR::Int16)
      {
      return TR::irdbar;
      }

   return OMR::IL::opCodeForDirectReadBarrier(dt);
   }

TR::ILOpCodes
J9::IL::opCodeForDirectLoad(TR::DataType dt)
   {
   if (dt == TR::Int8 || dt == TR::Int16)
      {
      return TR::iload;
      }

   if (dt.isOMRDataType())
      {
      return OMR::IL::opCodeForDirectLoad(dt);
      }

   return J9::IL::opCodesForDirectLoad[dt - TR::FirstJ9Type];
   }

TR::ILOpCodes
J9::IL::opCodeForDirectStore(TR::DataType dt)
   {
   if (dt == TR::Int8 || dt == TR::Int16)
      {
      return TR::istore;
      }

   if (dt.isOMRDataType())
      {
      return OMR::IL::opCodeForDirectStore(dt);
      }

   return J9::IL::opCodesForDirectStore[dt - TR::FirstJ9Type];
   }

TR::ILOpCodes
J9::IL::opCodeForDirectWriteBarrier(TR::DataType dt)
   {
   if (dt == TR::Int8 || dt == TR::Int16)
      {
      return TR::iwrtbar;
      }

   return OMR::IL::opCodeForDirectWriteBarrier(dt);
   }

TR::ILOpCodes
J9::IL::opCodeForIndirectReadBarrier(TR::DataType dt)
   {
   if (dt == TR::Int8 || dt == TR::Int16)
      {
      return TR::irdbari;
      }

   return OMR::IL::opCodeForIndirectReadBarrier(dt);
   }

TR::ILOpCodes
J9::IL::opCodeForIndirectLoad(TR::DataType dt)
   {
   if (dt == TR::Int8 || dt == TR::Int16)
      {
      return TR::iloadi;
      }

   if (dt.isOMRDataType())
      {
      return OMR::IL::opCodeForIndirectLoad(dt);
      }

   return J9::IL::opCodesForIndirectLoad[dt - TR::FirstJ9Type];
   }

TR::ILOpCodes
J9::IL::opCodeForIndirectStore(TR::DataType dt)
   {
   if (dt == TR::Int8 || dt == TR::Int16)
      {
      return TR::istorei;
      }

   if (dt.isOMRDataType())
      {
      return OMR::IL::opCodeForIndirectStore(dt);
      }

   return J9::IL::opCodesForIndirectStore[dt - TR::FirstJ9Type];
   }

TR::ILOpCodes
J9::IL::opCodeForIndirectWriteBarrier(TR::DataType dt)
   {
   if (dt == TR::Int8 || dt == TR::Int16)
      {
      return TR::iwrtbari;
      }

   return OMR::IL::opCodeForIndirectWriteBarrier(dt);
   }


TR::ILOpCodes
J9::IL::opCodeForIndirectArrayLoad(TR::DataType dt)
   {
   if (dt.isOMRDataType())
      {
      return OMR::IL::opCodeForIndirectArrayLoad(dt);
      }

   return J9::IL::opCodesForIndirectArrayLoad[dt - TR::FirstJ9Type];
   }

TR::ILOpCodes
J9::IL::opCodeForIndirectArrayStore(TR::DataType dt)
   {
   if (dt.isOMRDataType())
      {
      return OMR::IL::opCodeForIndirectArrayStore(dt);
      }

   return J9::IL::opCodesForIndirectArrayStore[dt - TR::FirstJ9Type];
   }

TR::ILOpCodes
J9::IL::opCodeForRegisterLoad(TR::DataType dt)
   {
   if (dt.isOMRDataType())
      {
      return OMR::IL::opCodeForRegisterLoad(dt);
      }

   return J9::IL::opCodesForRegisterLoad[dt - TR::FirstJ9Type];
   }

TR::ILOpCodes
J9::IL::opCodeForRegisterStore(TR::DataType dt)
   {
   if (dt.isOMRDataType())
      {
      return OMR::IL::opCodeForRegisterStore(dt);
      }

   return J9::IL::opCodesForRegisterStore[dt - TR::FirstJ9Type];
   }

TR::ILOpCodes
J9::IL::opCodeForCompareEquals(TR::DataType dt)
   {
   if (dt == TR::Int8 || dt == TR::Int16)
      {
      return TR::icmpeq;
      }

   if (dt.isOMRDataType())
      {
      return OMR::IL::opCodeForCompareEquals(dt);
      }

   return J9::IL::opCodesForCompareEquals[dt - TR::FirstJ9Type];
   }

TR::ILOpCodes
J9::IL::opCodeForCompareNotEquals(TR::DataType dt)
   {
   if (dt == TR::Int8 || dt == TR::Int16)
      {
      return TR::icmpne;
      }

   if (dt.isOMRDataType())
      {
      return OMR::IL::opCodeForCompareNotEquals(dt);
      }

   return J9::IL::opCodesForCompareNotEquals[dt - TR::FirstJ9Type];
   }

