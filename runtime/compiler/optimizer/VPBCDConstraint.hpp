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

#ifndef BCDVPCONSTRAINT_INCL
#define BCDVPCONSTRAINT_INCL

#include "optimizer/VPConstraint.hpp"


// NOTE: when adding new TR_BCDSignConstraint types
//       - update TR_BCDSignConstraintNames in VPConstraint.cpp when adding
//       - update bcdSignToSignConstraintMap, signConstraintToBCDSignMap and signConstraintMergeMap in this file
enum TR_BCDSignConstraint
   {
   TR_Sign_Unknown      = 0,
   TR_Sign_Clean        = 1,                 // preferred and no negative zero
   TR_Sign_Preferred    = 2,
   TR_FirstExactSign    = 3,
   TR_Sign_Plus         = TR_FirstExactSign, // also clean and preferred
   TR_Sign_Minus        = 4,                 // also preferred
   TR_Sign_Unsigned     = 5,
   TR_Sign_Minus_Clean  = 6,                 // also clean and preferred
   TR_LastExactSign     = TR_Sign_Minus_Clean,
   TR_Sign_Num_Types,
   };

// BCD Value Constraints

const TR_BCDSignConstraint bcdSignToSignConstraintMap[num_bcd_sign_codes] =   {
                                                                              TR_Sign_Unknown,  // bcd_invalid_sign
                                                                              TR_Sign_Plus,     // bcd_plus
                                                                              TR_Sign_Minus,    // bcd_minus
                                                                              TR_Sign_Unsigned, // bcd_unsigned
                                                                              };

const TR_BCDSignCode signConstraintToBCDSignMap[TR_Sign_Num_Types] =   {
                                                                       bcd_invalid_sign, // TR_Sign_Unknown
                                                                       bcd_invalid_sign, // TR_Sign_Clean
                                                                       bcd_invalid_sign, // TR_Sign_Preferred
                                                                       bcd_plus,         // TR_Sign_Plus
                                                                       bcd_minus,        // TR_Sign_Minus
                                                                       bcd_unsigned,     // TR_Sign_Unsigned
                                                                       bcd_minus,        // TR_Sign_Minus_Clean
                                                                       };

// Given any two sign constraints what is the conservative (i.e. merged) constraint that both satisfy.
// For example if constraint1 = Plus and constraint2 = Minus then the common constraint is Preferred as both Plus and Minus are preferred signs
const TR_BCDSignConstraint signConstraintMergeMap[TR_Sign_Num_Types][TR_Sign_Num_Types] =
 //                         Unknown          Clean             Preferred           Plus               Minus              Unsigned          Minus_Clean
   {
/* TR_Sign_Unknown */      {TR_Sign_Unknown, TR_Sign_Unknown,   TR_Sign_Unknown,   TR_Sign_Unknown,   TR_Sign_Unknown,   TR_Sign_Unknown,  TR_Sign_Unknown},
/* TR_Sign_Clean */        {TR_Sign_Unknown, TR_Sign_Clean,     TR_Sign_Preferred, TR_Sign_Clean,     TR_Sign_Preferred, TR_Sign_Unknown,  TR_Sign_Clean},
/* TR_Sign_Preferred */    {TR_Sign_Unknown, TR_Sign_Preferred, TR_Sign_Preferred, TR_Sign_Preferred, TR_Sign_Preferred, TR_Sign_Unknown,  TR_Sign_Preferred},
/* TR_Sign_Plus */         {TR_Sign_Unknown, TR_Sign_Clean,     TR_Sign_Preferred, TR_Sign_Plus,      TR_Sign_Preferred, TR_Sign_Unknown,  TR_Sign_Clean},
/* TR_Sign_Minus */        {TR_Sign_Unknown, TR_Sign_Preferred, TR_Sign_Preferred, TR_Sign_Preferred, TR_Sign_Minus,     TR_Sign_Unknown,  TR_Sign_Minus},
/* TR_Sign_Unsigned */     {TR_Sign_Unknown, TR_Sign_Unknown,   TR_Sign_Unknown,   TR_Sign_Unknown,   TR_Sign_Unknown,   TR_Sign_Unsigned, TR_Sign_Unknown},
/* TR_Sign_Minus_Clean */  {TR_Sign_Unknown, TR_Sign_Clean,     TR_Sign_Preferred, TR_Sign_Clean,     TR_Sign_Minus,     TR_Sign_Unknown,  TR_Sign_Minus_Clean},
   };

namespace TR {

class VP_BCDSign : public TR::VPConstraint
   {
   public:
   VP_BCDSign(TR_BCDSignConstraint sign, TR::DataType dt, OMR::ValuePropagation *vp)
      : TR::VPConstraint(BCDPriority), _dataType(dt), _sign(sign) { }

   static TR::VP_BCDSign *create(OMR::ValuePropagation *vp, TR_BCDSignConstraint sign, TR::DataType dt);
   virtual TR::VP_BCDSign *asBCDSign();

   static bool signsAreConsistent(TR_BCDSignConstraint signOne, TR_BCDSignConstraint signTwo);

   char *getName()
      { return getName(_sign); }

   static char *getName(TR_BCDSignConstraint sign)
      { return ((sign < TR_Sign_Num_Types) ? TR_BCDSignConstraintNames[sign] : (char*)"invalid_sign_constraint"); }

   static TR_BCDSignConstraint getSignConstraintFromBCDSign(TR_BCDSignCode bcdSign)
      {
      TR_BCDSignConstraint signConstraint = TR_Sign_Unknown;
      if (bcdSign >=0 && bcdSign < num_bcd_sign_codes)
         signConstraint = bcdSignToSignConstraintMap[bcdSign];
      return signConstraint;
      }

   static bool isExactSign(TR_BCDSignConstraint sign)
      {
      if (sign >= TR_FirstExactSign && sign <= TR_LastExactSign)
         return true;
      else
         return false;
      }

   static TR_BCDSignCode getBCDSignFromSignConstraint(TR_BCDSignConstraint signConstraint)
      {
      TR_BCDSignCode bcdSign = bcd_invalid_sign;
      if (isExactSign(signConstraint))
         bcdSign = signConstraintToBCDSignMap[signConstraint];
      return bcdSign;
      }

   static TR_BCDSignConstraint getMergedSignConstraint(TR_BCDSignConstraint signConstraintOne, TR_BCDSignConstraint signConstraintTwo)
      {
      if (signConstraintOne < TR_Sign_Num_Types && signConstraintTwo < TR_Sign_Num_Types)
         {
         return signConstraintMergeMap[signConstraintOne][signConstraintTwo];
         }
      else
         {
         return TR_Sign_Unknown;
         }
      }

   TR_BCDSignConstraint getSign() { return _sign; }
   TR::DataType getDataType() { return _dataType; }

   bool isEqualTo(TR_BCDSignConstraint otherSign, TR::DataType otherDataType);

   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();

   protected:
   TR_BCDSignConstraint _sign;
   TR::DataType _dataType;

   private:
   static char  *TR_BCDSignConstraintNames[TR_Sign_Num_Types];
   };

class VP_BCDValue : public TR::VP_BCDSign
   {
   public:
   VP_BCDValue(char *lit, int32_t litLowSize, char *str, TR_BCDSignConstraint sign, TR::DataType dt, OMR::ValuePropagation *vp);
   virtual TR::VP_BCDValue *asBCDValue();

   char *getLit()          { return _litLow; }
   int32_t getLitSize()    { return _litLowSize; }
   char *getStr()          { return _strLow; }
   int32_t getPrecision()  { return _lowPrecision; }

   char *getStrLow()          { return _strLow; }
   char *getLitLow()          { return _litLow; }
   int32_t getLitLowSize()    { return _litLowSize; }
   int32_t getLowPrecision()  { return _lowPrecision; }

   virtual char *getStrHigh() = 0;
   virtual char *getLitHigh() = 0;
   virtual int32_t getLitHighSize() = 0;

   static uint32_t hash(char *bcdLit, int32_t litSize, TR_BCDSignConstraint sign, OMR::ValuePropagation *vp);

   bool isEqualTo(int32_t otherPrecision, char *otherLit, int32_t otherLitSize, TR_BCDSignConstraint otherSign, TR::DataType otherDataType);
   virtual bool isHighEqualTo(int32_t otherPrecision, char *otherLit, int32_t otherLitSize, TR_BCDSignConstraint otherSign, TR::DataType otherDataType) = 0;
   static bool compareBCDConstraints(int32_t precisionOne, char *litOne, int32_t litSizeOne, TR_BCDSignConstraint signOne, TR::DataType dataTypeOne,
                                     int32_t precisionTwo, char *litTwo, int32_t litSizeTwo, TR_BCDSignConstraint signTwo, TR::DataType dataTypeTwo);

   protected:
   char *_litLow; // datatype dependent memory representation -- for packed 123C
   char *_strLow; // printable string -- "+123"
   int32_t _litLowSize;
   int32_t _lowPrecision;

   };

}

#endif
