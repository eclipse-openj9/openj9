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

#include "optimizer/J9SimplifierHelpers.hpp"
#include "optimizer/Simplifier.hpp"

#include <limits.h>
#include <math.h>
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Block.hpp"

bool propagateSignState(TR::Node *node, TR::Node *child, int32_t shiftAmount, TR::Block *block, TR::Simplifier *s)
   {
   bool changedSignState = false;
   if (!node->hasKnownOrAssumedSignCode() &&
       child->hasKnownOrAssumedSignCode() &&
       TR::Node::typeSupportedForSignCodeTracking(node->getDataType()) &&
       performTransformation(s->comp(),"%sTransfer %sSignCode 0x%x from %s [" POINTER_PRINTF_FORMAT "] to %s [" POINTER_PRINTF_FORMAT "]\n",
         s->optDetailString(),
         child->hasKnownSignCode() ? "Known":"Assumed",
         TR::DataType::getValue(child->getKnownOrAssumedSignCode()),
         child->getOpCode().getName(),
         child,
         node->getOpCode().getName(),
         node))
      {
      node->transferSignCode(child);
      changedSignState = true;
      }

   if (!node->hasKnownOrAssumedCleanSign() &&
       child->hasKnownOrAssumedCleanSign() &&
       ((node->getDecimalPrecision() >= child->getDecimalPrecision() + shiftAmount) || child->isNonNegative()) &&
         performTransformation(s->comp(), "%sSet Has%sCleanSign=true on %s [" POINTER_PRINTF_FORMAT "] due to %s already clean %schild %s [" POINTER_PRINTF_FORMAT "]\n",
            s->optDetailString(),
            child->hasKnownCleanSign()?"Known":"Assumed",
            node->getOpCode().getName(),
            node,
            !child->isNonNegative()?"a widening of":"an",
            child->isNonNegative()?">= zero ":"",
            child->getOpCode().getName(),
            child))
      {
      node->transferCleanSign(child);
      changedSignState = true;
      }

   return changedSignState;
   }

bool propagateSignStateUnaryConversion(TR::Node *node, TR::Block *block, TR::Simplifier *s)
   {
   bool validateOp = node->getType().isBCD() &&
                     ((node->getOpCode().isConversion() && node->getNumChildren()==1)
                      || (node->getOpCode().isConversion() && node->getOpCode().canHavePaddingAddress() && node->getNumChildren()==2)
                     );
   if (!validateOp)
      return false;

   TR::Node *child = node->getFirstChild();
   return propagateSignState(node, child, 0, block, s);
   }

TR::Node *removeOperandWidening(TR::Node *node, TR::Node *parent, TR::Block *block, TR::Simplifier * s)
   {
   if (s->comp()->getOption(TR_KeepBCDWidening))   // stress-testing option to force more operations through to the evaluators
      return node;

   // Many packed decimal node types (such as arithmetic/shift/stores) do not need their operands explicitly widened so simple
   // widening operations (i.e. pdshl by 0 nodes) are be removed here.
   // This cannot be done globally because other node types (such a packed node under a call) must be explicitly widened.
   if (node->isSimpleWidening())
      {
      return s->replaceNodeWithChild(node, node->getFirstChild(), s->_curTree, block, false); // correctBCDPrecision=false because node is a widening
      }
   else if ((node->getOpCodeValue() == TR::i2pd || node->getOpCodeValue() == TR::l2pd) &&
            node->hasSourcePrecision() &&
            node->getReferenceCount() == 1 && // the removal of widening may not be valid in the other contexts
            node->getDecimalPrecision() > node->getSourcePrecision() &&
            performTransformation(s->comp(), "%sReducing %s [" POINTER_PRINTF_FORMAT "] precision %d to its child integer precision of %d\n",
               s->optDetailString(), node->getOpCode().getName(), node, node->getDecimalPrecision(), node->getSourcePrecision()))
      {
      node->setDecimalPrecision(node->getSourcePrecision());
      }
   else if (node->getOpCode().isShift() &&
            node->getReferenceCount() == 1 &&
            node->getSecondChild()->getOpCode().isLoadConst())
      {
      int32_t adjust = node->getDecimalAdjust(); // adjust is < 0 for right shifts and >= 0 for left shifts
      int32_t maxShiftedPrecision = adjust+node->getFirstChild()->getDecimalPrecision();
      if (node->getOpCode().isPackedRightShift() &&
          node->getDecimalRound() != 0)
         {
         maxShiftedPrecision++; // +1 as the rounding can propagate an extra digit across
         }
      if ((maxShiftedPrecision > 0) &&
          (node->getDecimalPrecision() > maxShiftedPrecision) &&
          performTransformation(s->comp(), "%sReducing %s [" POINTER_PRINTF_FORMAT "] precision %d to the max shifted result precision of %d\n",
            s->optDetailString(), node->getOpCode().getName(), node, node->getDecimalPrecision(), maxShiftedPrecision))
         {
         bool signWasKnownClean = node->hasKnownCleanSign();
         bool signWasAssumedClean = node->hasAssumedCleanSign();

         node->setDecimalPrecision(maxShiftedPrecision);    // conservatively resets clean sign flags on a truncation -- but this truncation cannot actually dirty a sign

         if (signWasKnownClean)
            node->setHasKnownCleanSign(true);
         if (signWasAssumedClean)
            node->setHasAssumedCleanSign(true);
         }
      }
   // TODO: figure out the more global check for removing widenings - all unary operations (getNumChildren()==1), how about setSign and other shifts?
   else if ((node->getOpCodeValue() == TR::pdclean   ||
             node->getOpCodeValue() == TR::zd2zdsle  ||
             node->getOpCodeValue() == TR::zdsle2zd  ||
             node->getOpCodeValue() == TR::zd2pd     ||
             node->getOpCodeValue() == TR::pd2zd     ||
             node->getOpCodeValue() == TR::pdSetSign ||
             node->getOpCodeValue() == TR::pdclear   ||
             node->getOpCodeValue() == TR::pdclearSetSign) &&
            node->getReferenceCount() == 1 &&
            node->getDecimalPrecision() > node->getFirstChild()->getDecimalPrecision() &&
            performTransformation(s->comp(), "%sReducing %s [" POINTER_PRINTF_FORMAT "] precision %d to its child precision of %d\n",
               s->optDetailString(), node->getOpCode().getName(), node, node->getDecimalPrecision(), node->getFirstChild()->getDecimalPrecision()))
      {
      node->setDecimalPrecision(node->getFirstChild()->getDecimalPrecision());
      if (node->getOpCode().isConversion())
         propagateSignStateUnaryConversion(node, block, s);
      return s->simplify(node, block);
      }
   return node;
   }

