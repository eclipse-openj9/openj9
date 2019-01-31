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

#include "optimizer/VPBCDConstraint.hpp"

#include <ctype.h>
#include <stddef.h>
#include "codegen/FrontEnd.hpp"
#include "env/KnownObjectTable.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/VMJ9.h"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/IO.hpp"
#include "env/ObjectModel.hpp"
#include "env/PersistentInfo.hpp"
#include "env/jittypes.h"
#include "env/VMAccessCriticalSection.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "ilgen/IlGenRequest.hpp"
#include "ilgen/IlGeneratorMethodDetails.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/J9ValuePropagation.hpp"
#include "runtime/RuntimeAssumptions.hpp"

// ***************************************************************************
//
// Methods of Value Propagation Constraints
//
// ***************************************************************************
TR::VP_BCDValue         *TR::VP_BCDValue::asBCDValue()                  { return this; }
TR::VP_BCDSign          *TR::VP_BCDSign::asBCDSign()                    { return this; }


char *TR::VP_BCDSign::TR_BCDSignConstraintNames[TR_Sign_Num_Types] =
   {
   "<unknown_sign_state>",
   "<clean>",
   "<preferred>",
   "<plus>",
   "<minus>",
   "<unsigned>",
   "<minus_clean>"
   };

TR::VP_BCDValue::VP_BCDValue(char *lit, int32_t litSize, char *str, TR_BCDSignConstraint sign, TR::DataType dt, OMR::ValuePropagation *vp)
      : TR::VP_BCDSign(sign, dt, vp), _litLowSize(litSize)
   {
   _litLow = (char *)vp->comp()->trMemory()->allocateStackMemory(litSize);
   memcpy(_litLow,lit,litSize);

   if (dt == TR::Aggregate || dt.isBCD())
      {
      TR_ASSERT(str,"string should not be NULL for BCD/Aggr type %s\n",dt.toString());
      // BCD/Aggr types already carry around a string representation on the node so it is privatized to the constraint here
      // (in case the node itself is changed)
      _strLow = (char *)vp->comp()->trMemory()->allocateStackMemory(strlen(str)+1); // +1 for terminating null
      strcpy(_strLow,str);
      _lowPrecision = dt.isBCD() ? TR::DataType::getBCDPrecisionFromString(str, dt) : litSize;
      }
   else
      {
      TR_ASSERT(false,"unexpected dt %s in TR::VP_BCDValue\n",dt.toString());
      }
   }

bool
TR::VP_BCDValue::compareBCDConstraints(int32_t precisionOne, char *litOne, int32_t litSizeOne, TR_BCDSignConstraint signOne, TR::DataType dataTypeOne,
                                      int32_t precisionTwo, char *litTwo, int32_t litSizeTwo, TR_BCDSignConstraint signTwo, TR::DataType dataTypeTwo)
   {
   if (dataTypeOne != dataTypeTwo)
      return false;

   if (precisionOne != precisionTwo)
      return false;

   if (litSizeOne != litSizeTwo)
      return false;

   if (signOne != signTwo)
      return false;

   if (memcmp(litOne, litTwo, litSizeOne) == 0)
      return true;
   else
      return false;
   }

bool
TR::VP_BCDValue::isEqualTo(int32_t otherPrecision, char *otherLit, int32_t otherLitSize, TR_BCDSignConstraint otherSign, TR::DataType otherDataType)
   {
   return TR::VP_BCDValue::compareBCDConstraints(getPrecision(), getLit(), getLitSize(), getSign(), getDataType(),
                                                otherPrecision, otherLit, otherLitSize, otherSign, otherDataType);
   }

#define TR_PLUS_HASH    5
#define TR_UNSIGNED_HASH 13
#define TR_MINUS_HASH   11

uint32_t TR::VP_BCDValue::hash(char *lit, int32_t litSize, TR_BCDSignConstraint sign, OMR::ValuePropagation *vp)
   {
   uint32_t hash = (uint32_t)sign;
   hash += TR::Node::hashOnBCDOrAggrLiteral(lit, litSize);
   return hash;
   }

TR::VP_BCDSign *TR::VP_BCDSign::create(OMR::ValuePropagation *vp, TR_BCDSignConstraint type, TR::DataType dt)
   {
   TR_ASSERT(type < TR_Sign_Num_Types,"invalid TR_BCDSignConstraint type %d\n",type);
   J9::ValuePropagation *j9vp = static_cast<J9::ValuePropagation *>(vp);
   TR::VP_BCDSign **bcdSignConstraints = j9vp->getBCDSignConstraints(dt);
   TR::VP_BCDSign *constraint = type < TR_Sign_Num_Types ? bcdSignConstraints[type] : NULL;
   if (constraint)
      {
      if (vp->trace())
         traceMsg(vp->comp(),"return existing BCD sign constraint %p (dt=%s, sign=%s)\n",constraint,TR::DataType::getName(dt),constraint->getName());
      return constraint;
      }

   constraint = new (vp->trStackMemory()) TR::VP_BCDSign(type, dt, vp);

   bcdSignConstraints[type] = constraint;

   if (vp->trace())
      traceMsg(vp->comp(),"created new BCD sign constraint %p (dt=%s, sign=%s)\n",constraint,TR::DataType::getName(dt),constraint->getName());

   return constraint;
   }

bool TR::VP_BCDSign::isEqualTo(TR_BCDSignConstraint otherSign, TR::DataType otherDataType)
   {
   if (getSign()     == otherSign &&
       getDataType() == otherDataType)
      {
      return true;
      }
   else
      {
      return false;
      }
   }

bool checkPlus(TR_BCDSignConstraint signOne, TR_BCDSignConstraint signTwo)
   {
   if (signOne == TR_Sign_Plus)
      {
      if (signTwo == TR_Sign_Minus || signTwo == TR_Sign_Unsigned || signTwo == TR_Sign_Minus_Clean)
         return false;
      }
   return true;
   }

bool checkMinus(TR_BCDSignConstraint signOne, TR_BCDSignConstraint signTwo)
   {
   if (signOne == TR_Sign_Minus)
      {
      if (signTwo == TR_Sign_Plus || signTwo == TR_Sign_Unsigned)
         return false;
      }
   return true;
   }

bool checkUnsigned(TR_BCDSignConstraint signOne, TR_BCDSignConstraint signTwo)
   {
   if (signOne == TR_Sign_Unsigned)
      {
      if (signTwo == TR_Sign_Minus || signTwo == TR_Sign_Plus || signTwo == TR_Sign_Minus_Clean)
         return false;
      }
   return true;
   }

bool TR::VP_BCDSign::signsAreConsistent(TR_BCDSignConstraint signOne, TR_BCDSignConstraint signTwo)
   {
   if (checkPlus(signOne, signTwo) &&
       checkMinus(signOne, signTwo) &&
       checkUnsigned(signOne, signTwo) &&
       checkPlus(signTwo, signOne) &&
       checkMinus(signTwo, signOne) &&
       checkUnsigned(signTwo, signOne))
      {
      return true;
      }
   else
      {
      return false;
      }
   }

TR::VPConstraint *TR::VP_BCDSign::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   if (vp->trace())
      {
      // Tracer makes this redundant really
      traceMsg(vp->comp(),"\nTR::VP_BCDSign::intersect1\n");
      traceMsg(vp->comp(),"this  %p: ",this);
      print(vp->comp(), vp->comp()->getOutFile());
      traceMsg(vp->comp(), "\n");
      traceMsg(vp->comp(),"other %p: ",other);
      other->print(vp->comp(), vp->comp()->getOutFile());
      traceMsg(vp->comp(), "\n");
      }

   if (asBCDSign() && other->asBCDSign())
      {
      TR_ASSERT(TR::VP_BCDSign::signsAreConsistent(asBCDSign()->getSign(), other->asBCDSign()->getSign()),
         "sign mismatch %s != %s\n",TR::VP_BCDSign::getName(asBCDSign()->getSign()),TR::VP_BCDSign::getName(other->asBCDSign()->getSign()));
      }
   return NULL;
   }

TR::VPConstraint *TR::VP_BCDSign::merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   if (vp->trace())
      {
      // Tracer makes this redundant really
      traceMsg(vp->comp(),"\nTR::VP_BCDSign::merge1\n");
      traceMsg(vp->comp(),"this  %p: ",this);
      print(vp->comp(), vp->comp()->getOutFile());
      traceMsg(vp->comp(), "\n");
      traceMsg(vp->comp(),"other %p (isBCDSign=%d): ",other,other->asBCDSign()?1:0);
      other->print(vp->comp(), vp->comp()->getOutFile());
      traceMsg(vp->comp(), "\n");
      }

   if (asBCDSign() && other->asBCDSign())
      {
      TR_BCDSignConstraint thisSign = asBCDSign()->getSign();
      TR_BCDSignConstraint otherSign = other->asBCDSign()->getSign();

      if (vp->trace())
         traceMsg(vp->comp(),"\tthisSign %s thisType %s, otherSign %s otherType %s\n",
            asBCDSign()->getName(),TR::DataType::getName(asBCDSign()->getDataType()),other->asBCDSign()->getName(),TR::DataType::getName(other->asBCDSign()->getDataType()));

      if (thisSign == TR_Sign_Unknown || otherSign == TR_Sign_Unknown)
         {
         if (vp->trace())
            traceMsg(vp->comp(),"\tone of the signs is unknown -- return NULL\n",asBCDSign()->getName(),other->asBCDSign()->getName());
         return NULL;
         }

      if (asBCDSign()->getDataType() != other->asBCDSign()->getDataType())
         {
         if (vp->trace())
            traceMsg(vp->comp(),"\ttypes do not match -- return NULL\n",asBCDSign()->getName(),other->asBCDSign()->getName());
         return NULL;
         }

      TR_BCDSignConstraint mergedSign = TR::VP_BCDSign::getMergedSignConstraint(thisSign, otherSign);
      if (vp->trace())
         traceMsg(vp->comp(),"\tmergedSign = %s from %s x %s\n",TR::VP_BCDSign::getName(mergedSign),TR::VP_BCDSign::getName(thisSign),TR::VP_BCDSign::getName(otherSign));
      if (mergedSign != TR_Sign_Unknown)
         {
         TR::VP_BCDSign *mergedSignConstraint = TR::VP_BCDSign::create(vp, mergedSign, asBCDSign()->getDataType());
         if (vp->trace())
            {
            traceMsg(vp->comp(),"\treturn mergedSignConstraint %p of sign %s: ",mergedSignConstraint,mergedSignConstraint->getName());
            mergedSignConstraint->print(vp->comp(), vp->comp()->getOutFile());
            traceMsg(vp->comp(), "\n\n");
            }
         return mergedSignConstraint;
         }
      }

   if (vp->trace())
      traceMsg(vp->comp(),"\tcannot merge BCDSign constraints -- return NULL\n\n");
   return NULL;
   }

void TR::VP_BCDSign::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
    trfprintf(outFile, "%s (%s)", getName(), TR::DataType::getName(getDataType()));
   }

const char *TR::VP_BCDSign::name()                   { return "BCDSign";              }


