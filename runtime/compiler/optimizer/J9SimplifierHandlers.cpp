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

#include "optimizer/OMRSimplifierHelpers.hpp"
#include "optimizer/OMRSimplifierHandlers.hpp"
#include "optimizer/J9SimplifierHelpers.hpp"
#include "optimizer/J9SimplifierHandlers.hpp"
#include "optimizer/Simplifier.hpp"

#include <algorithm>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/bitvectr.h"
#include "cs2/hashtab.h"
#include "env/IO.hpp"
#include "env/ObjectModel.hpp"
#include "env/TRMemory.hpp"
#include "env/VMJ9.h"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "infra/Cfg.hpp"
#include "infra/HashTab.hpp"
#include "infra/List.hpp"
#include "infra/TRCfgEdge.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/Simplifier.hpp"
#include "optimizer/TransformUtil.hpp"


enum TR_PackedCompareCondition
   {
   TR_PACKED_COMPARE_EQUAL    = 0, //  negative 0 is considered equal to positive zero
   TR_PACKED_COMPARE_LESS     = 1,
   TR_PACKED_COMPARE_GREATER  = 2,
   TR_PACKED_COMPARE_UNKNOWN  = 3,
   TR_PACKED_COMPARE_TYPES    = 4
   };

// Forward declarations.
static TR::Node *foldSetSignIntoNode(TR::Node *setSign, bool setSignIsTheChild, TR::Node *other, bool removeSetSign, TR::Block *block, TR::Simplifier *s);
static TR::Node *foldAndReplaceDominatedSetSign(TR::Node *setSign, bool setSignIsTheChild, TR::Node *other, TR::Block * block, TR::Simplifier * s);
static TR::Node *createSetSignForKnownSignChild(TR::Node *node, TR::Block * block, TR::Simplifier * s);

static TR::Node *removeUnnecessaryDFPClean(TR::Node *child, TR::Node *parent, TR::Block *block, TR::Simplifier *s)
   {
   TR::ILOpCodes dfpCleanOp = TR::ILOpCode::cleanOpCode(child->getDataType());
   if (child->getOpCodeValue() == dfpCleanOp &&
       performTransformation(s->comp(), "%s%s [" POINTER_PRINTF_FORMAT "] does not need DFP cleaning child %s [" POINTER_PRINTF_FORMAT "] -- remove child [" POINTER_PRINTF_FORMAT "]\n",
         s->optDetailString(),parent->getOpCode().getName(),parent,child->getOpCode().getName(),child,child))
      {
      // Transform:
      // dd2zdSetSign (or ifdxcmpxx)
      //    ddclean
      //       ddX
      // to:
      // dd2zdSetSign (or ifdxcmpxx)
      //    ddX
      child = s->replaceNodeWithChild(child, child->getFirstChild(), s->_curTree, block);
      }
   return child;
   }

static void propagateNonNegativeFlagForArithmetic(TR::Node *node, TR::Simplifier *s)
   {
   if (!node->isNonNegative() &&
       node->getFirstChild()->isNonNegative() && node->getSecondChild()->isNonNegative() &&
       performTransformation(s->comp(),"%sSet X>=0 flag on %s [" POINTER_PRINTF_FORMAT "] due to both children X>=0\n",s->optDetailString(),node->getOpCode().getName(),node))
      {
      node->setIsNonNegative(true);
      }
   }

// Find a DFP setsign op (ddabs or ddSetNegative) that's below a parent
// node without any intermediate nodes that make use of the sign (eg. add)
static TR::Node *getRedundantDFPSetSignDescendant(TR::Node *node, TR::Node **parent)
   {
   TR::ILOpCodes op = node->getOpCodeValue();

   if (!node->getType().isDFP() || node->getReferenceCount() > 1)
      return NULL;
   if (op == TR::dfabs || op == TR::ddabs || op == TR::deabs || op == TR::dfSetNegative || op == TR::ddSetNegative || op == TR::deSetNegative)
      return node;
   if (node->getOpCode().isConversion())
      return NULL;
   if (node->getOpCode().isAdd() || node->getOpCode().isSub() || node->getOpCode().isMul() || node->getOpCode().isDiv())
      return NULL;

   if (node->getNumChildren() < 1)
      return NULL;

   *parent = node;
   return getRedundantDFPSetSignDescendant(node->getFirstChild(), parent);
   }

static TR::Node *foldSetSignFromGrandChild(TR::Node *node, TR::Block *block, TR::Simplifier *s)
   {
   // set sign operations are typically cheaper then the corresponding general sign moving operation (eg. zdshrSetSign vs zdshr).

   TR::Node *child = node->getFirstChild();
   TR::ILOpCodes newSetSignOp = TR::ILOpCode::setSignVersionOfOpCode(node->getOpCodeValue());
   if (node->getReferenceCount() == 1 &&
       newSetSignOp != TR::BadILOp &&
       child->getReferenceCount() == 1 &&
       (child->getOpCodeValue() == TR::zd2pd || child->getOpCodeValue() == TR::pd2zd) &&
       child->getFirstChild()->getReferenceCount() == 1 &&
       child->getFirstChild()->getOpCode().isSetSign())
      {
      TR::Node *grandChild = child->getFirstChild();
      int32_t grandChildSetSignIndex = TR::ILOpCode::getSetSignValueIndex(grandChild->getOpCodeValue());
      TR::Node *grandChildSetSign = child->getFirstChild()->getChild(grandChildSetSignIndex);

      int32_t convertedSetSign = TR::DataType::getInvalidSignCode();
      if (grandChildSetSign->getOpCode().isLoadConst())
         {
         int32_t originalSetSign = grandChildSetSign->get32bitIntegralValue();
         convertedSetSign = TR::DataType::convertSignEncoding(grandChild->getDataType(), node->getDataType(), originalSetSign);
         TR_ASSERT(convertedSetSign != TR::DataType::getInvalidSignCode(),"could not convert sign encoding of 0x%x from type %s to type %s\n",originalSetSign,grandChild->getDataType().toString(),node->getDataType().toString());
         }
      bool removeSetSignGrandChild = grandChild->getOpCodeValue() == TR::pdSetSign;
      if (convertedSetSign != TR::DataType::getInvalidSignCode() &&
          performTransformation(s->comp(), "%sFold%s %s [" POINTER_PRINTF_FORMAT "] above parent pd2zd [" POINTER_PRINTF_FORMAT "] and into grandparent %s [" POINTER_PRINTF_FORMAT "] and create new ",
            s->optDetailString(),removeSetSignGrandChild?" and remove":"",child->getFirstChild()->getOpCode().getName(),child->getFirstChild(),child,node->getOpCode().getName(),node))
         {
         int32_t newSetSignindex = TR::ILOpCode::getSetSignValueIndex(newSetSignOp);
         TR::Node *newNode = NULL;
         TR::Node *newSetSign = TR::Node::iconst(node, convertedSetSign);
         if (newSetSignindex == 1)
            {
            // Transform:
            //    zd2zdsls
            //       pd2zd
            //          pdshlSetSign - src
            //             x
            //             shift
            //             sign
            // to:
            //       zd2zdslsSetSign
            //          pd2zd
            //             pdshlSetSign - src
            //                x
            //                shift
            //                newSign 0xf
            //          sign

            newNode = TR::Node::create(newSetSignOp, 2,
                                      child,                   // pd2zd
                                      newSetSign);
            child->decReferenceCount();
            }
         else
            {
            TR_ASSERT(false,"setSign index of %d not supported in foldSetSignFromGrandChild\n",newSetSignindex);
            }
         if (newNode)
            {
            dumpOptDetails(s->comp(), "%s node [" POINTER_PRINTF_FORMAT "]\n", newNode->getOpCode().getName(),newNode);
            newNode->incReferenceCount();
            newNode->setDecimalPrecision(node->getDecimalPrecision());
            stopUsingSingleNode(node, true, s); // removePadding=true
            child->setVisitCount(0); // revisit this node
            grandChildSetSign->recursivelyDecReferenceCount();
            if (removeSetSignGrandChild)
               {
               TR_ASSERT(grandChild->getNumChildren() == 2,"can only remove setSign grandChild node %p with two children and not %d children\n",grandChild,grandChild->getNumChildren());
               child->setChild(0, grandChild->getFirstChild());
               stopUsingSingleNode(grandChild, true, s); // removePadding=true
               }
            else
               {
               TR::Node *modifiedSetSign = TR::Node::iconst(grandChild, TR::DataType::getIgnoredSignCode());
               grandChild->setAndIncChild(grandChildSetSignIndex, modifiedSetSign);
               grandChild->resetSignState();
               }
            return newNode;
            }
         }
      }
   return node;
   }

static TR::Node *removeShiftTruncationForConversionParent(TR::Node *conversion, TR::Block * block, TR::Simplifier * s)
   {
   //if (!conversion->alwaysGeneratesAKnownCleanSign()) return child; // Allow for pd2udsx nodes too even though they could generate a negative unicode zero?

   // Look for a truncating child or grandchild shift under a pd2x. The truncation will likely be cheaper to perform as part of
   // the pd2x so increase the precision of the shift (and an intermediate clean if present) so it no longer truncates.
   TR::Node *child = conversion->getFirstChild();
   if (child->getReferenceCount() == 1)
      {
      TR::Node *pdclean = NULL;
      TR::Node *shift = NULL;
      if (child->getOpCodeValue() == TR::pdclean &&
          child->getFirstChild()->getReferenceCount() == 1 &&
          child->getFirstChild()->getOpCode().isPackedShift())
         {
         // conversion
         //    pdclean
         //       pdshx
         pdclean = child;
         shift = child->getFirstChild();
         }
      else if (child->getOpCode().isPackedShift())
         {
         // conversion
         //    pdshx
         shift = child;
         }
      if (shift &&
          conversion->getDecimalPrecision() == shift->getDecimalPrecision() &&                 // final precision matches truncated pdshl precision
          (!pdclean || pdclean->getDecimalPrecision() >= shift->getDecimalPrecision()))   // and there is no intermediate truncation by the pdclean (if there is a pdclean)
         {
         TR::Node *shiftSource = shift->getFirstChild();
         int32_t shiftedPrecision = shiftSource->getDecimalPrecision() + shift->getDecimalAdjust();
         if (shiftedPrecision <= TR::DataType::getMaxPackedDecimalPrecision() && // do not create large pdcleans or pdshrs that may not be supported by the code generator
             shiftedPrecision > conversion->getDecimalPrecision() &&
             performTransformation(s->comp(), "%sDelaying truncation until %s [" POINTER_PRINTF_FORMAT "] by increasing %s [" POINTER_PRINTF_FORMAT "] precision %d->%d",
               s->optDetailString(), conversion->getOpCode().getName(), conversion, shift->getOpCode().getName(), shift, shift->getDecimalPrecision(), shiftedPrecision))
            {
            if (pdclean)
               {
               dumpOptDetails(s->comp(), " and intermediate pdclean [" POINTER_PRINTF_FORMAT "] precision %d->%d", pdclean,pdclean->getDecimalPrecision(),shiftedPrecision);
               pdclean->setDecimalPrecision(shiftedPrecision);
               pdclean->setVisitCount(0);
               }
            dumpOptDetails(s->comp(),"\n");
            shift->setDecimalPrecision(shiftedPrecision);
            shift->setVisitCount(0);
            child->setVisitCount(0);
            return s->simplify(child, block); // try to propagate the shrunken precision down the tree
            }
         }
      }
   return child;
   }

static TR::Node *flipBinaryChildAndUnaryParent(TR::Node *parent, TR::Node *child, TR::ILOpCodes newParentOp, TR::Block *block, TR::Simplifier * s)
   {
   // Flip the parent/child order and change the new parent's (the old child's) opcode to newParentOp. Return the new parent node.
   // Transform:
   // parent
   //    child
   //       x
   //       y
   // To:
   // newParent (with newParentOp opcodeValue)
   //    parent
   //       x
   //    y
   TR_ASSERT(parent->getNumChildren() == 1 || parent->getNumChildren() == 2,"parent node %p must have one or two children and not %d children\n",parent,parent->getNumChildren());
   TR_ASSERT(child->getNumChildren() == 2,"child node must have exactly two children and not %d child(ren)\n",child->getNumChildren());

   int32_t parentPrec = TR::DataType::getInvalidDecimalPrecision();
   if (parent->getType().isBCD())
      parentPrec = parent->getDecimalPrecision();

   TR::Node *newChildren[2];
   newChildren[0] = TR::Node::create(parent->getOpCodeValue(), 1, child->getFirstChild());
   newChildren[1] = child->getSecondChild();
   child->incReferenceCount(); // Otherwise prepareToReplaceNode may wrongly dec refcounts of child's children
   s->prepareToReplaceNode(parent, newParentOp);
   parent->addChildren(newChildren, 2);
   if (parent->getType().isBCD())
      {
      TR_ASSERT(parentPrec != TR::DataType::getInvalidDecimalPrecision(),"expecting original parent node to be a BCD type\n"); // otherwise how do we get the precision value?
      parent->setDecimalPrecision(parentPrec);
      if (child->getType().isBCD())
         parent->getFirstChild()->setDecimalPrecision(parentPrec);
      }
   parent->setVisitCount(0);     // revisit this node

   // the parent may not handle truncation so insert a modifyPrecision op if needed
   TR::Node *newChild = child->getFirstChild();
   if (parent->getDecimalPrecision() < newChild->getDecimalPrecision())
      {
      // zdSetSign      - newParent
      //    pd2zd p=3   - parent
      //       pdX p=5  - origNewChild=newChild
      //    sign
      // To:
      // zdSetSign                     - newParent
      //    pd2zd  p=3                 - parent
      //       pdModifyPrecision p=3   - newChild = new mod prec child
      //          pdX p=5              - origNewChild
      //    sign
      TR::Node *origNewChild = newChild;
      TR::ILOpCodes modifyPrecisionOp = TR::ILOpCode::modifyPrecisionOpCode(origNewChild->getDataType());
      TR_ASSERT(modifyPrecisionOp != TR::BadILOp,"no bcd modify precision opcode found\n");
      newChild = TR::Node::create(origNewChild, modifyPrecisionOp, 1);
      newChild->setChild(0, origNewChild);
      newChild->setDecimalPrecision(parent->getDecimalPrecision());
      newChild->setReferenceCount(1);
      dumpOptDetails(s->comp(),"%sparent %s [" POINTER_PRINTF_FORMAT "] and newChild %s [" POINTER_PRINTF_FORMAT "] precision mismatch (%d < %d) so create a new %s [" POINTER_PRINTF_FORMAT "]\n",
         s->optDetailString(),parent->getOpCode().getName(),parent,origNewChild->getOpCode().getName(),origNewChild,parent->getDecimalPrecision(),origNewChild->getDecimalPrecision(),newChild->getOpCode().getName(),newChild);
      parent->getFirstChild()->setChild(0, newChild);
      }

   if (child->getReferenceCount() == 1)
      {
      stopUsingSingleNode(child, true, s); // removePadding=true
      child->getFirstChild()->decReferenceCount();
      child->getSecondChild()->decReferenceCount();
      }
   else
      {
      child->decReferenceCount();
      }
   parent->setChild(0, s->simplify(parent->getFirstChild(), block));
   return s->simplify(parent, block);
   }

static TR::Node *flipCleanAndShift(TR::Node *node, TR::Block * block, TR::Simplifier * s)
   {
   if ((node->getOpCodeValue() == TR::pdclean) &&
       (node->getFirstChild()->getOpCodeValue() == TR::pdshl) &&
       (!node->getFirstChild()->getFirstChild()->getOpCode().isConversion()) && // a conversion will likely have to initialize on its own so no benefit to trying to get the clean to do it
       (node->getFirstChild()->getSecondChild()->getOpCode().isLoadConst()))
      {
      TR::Node *pdclean = node;
      TR::Node *pdshl = node->getFirstChild();
      int32_t shiftAmount = pdshl->getSecondChild()->get32bitIntegralValue();
      int32_t maxShiftedPrecision = shiftAmount+pdshl->getFirstChild()->getDecimalPrecision();
      if (((shiftAmount&0x1) == 0) && (pdshl->getDecimalPrecision() >= maxShiftedPrecision) &&
           performTransformation(s->comp(), "%sCreate a new parent pdshl on pdclean [" POINTER_PRINTF_FORMAT "] and remove grandchild pdshl [" POINTER_PRINTF_FORMAT "]\n",
            s->optDetailString(), pdclean, pdshl))
         {
         // A pdclean can likely initialize and clean in one step so move it lower in the tree to increase the chances that it will perform the initialization
         // Note that this transformation is not done in the pdcleanSimplifier as we do not want to move the pdclean lower when the parent is pdstore because in this
         // case the clean and store can be done in one step.
         // Transform:
         // pd2zd
         //   pdclean
         //     pdshl
         //       x
         //       shift
         // To:
         // pd2zd
         //   pdshl
         //     pdclean
         //       x
         //     shift
         return flipBinaryChildAndUnaryParent(pdclean, pdshl, pdshl->getOpCodeValue(), block, s);
         }
      }
   return node;
   }

TR::Node *zdloadSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   return node;
   }

TR::Node *zdsleStoreSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   // It is not good to remove all widenings for zdsle children because then this store node will have to move the sign code from the
   // the narrowed child and clear (0xF0) the intermediate bytes.
   // It is better to just let the child do the widening so the sign code does not have to be moved.
   //
   //   TR::Node *valueChild = node->setValueChild(removeOperandWidening(node->getValueChild(), node, block, s));

   TR::Node *valueChild = node->getValueChild();
   if (valueChild->isSimpleWidening())
      {
      valueChild = node->setValueChild(s->replaceNodeWithChild(valueChild, valueChild->getFirstChild(), s->_curTree, block, false)); // correctBCDPrecision=false because node is a widening
      }

   return node;
   }

// Handles izdsts, zdsts
TR::Node *zdstsStoreSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node *valueChild = node->setValueChild(removeOperandWidening(node->getValueChild(), node, block, s));

   return node;
   }

//---------------------------------------------------------------------
// Zoned decimal to zoned decimal sign leading embedded
//
TR::Node *zd2zdsleSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));

   propagateSignStateUnaryConversion(node, block, s);
   TR::Node *child = node->getFirstChild();

   if (child->getOpCode().isSetSign())
      {
      TR::Node *newNode = foldSetSignIntoNode(child, true /* setSignIsTheChild */, node, true /* removeSetSign */, block, s);
      if (newNode != node)
         return newNode;
      }

   child = node->getFirstChild();
   TR::Node * result = NULL;
   if ((node->getDecimalPrecision() >= child->getDecimalPrecision()) &&
       (result = s->unaryCancelOutWithChild(node, child, s->_curTree, TR::zdsle2zd)))
      {
      return result;
      }

   return node;
   }

TR::Node *pd2udSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   node->setChild(0, removeShiftTruncationForConversionParent(node, block, s));

   TR::Node *child = node->getFirstChild();

   if (node->getDecimalPrecision() == child->getDecimalPrecision() &&
       child->isSimpleTruncation() &&
       performTransformation(s->comp(), "%sRemove simple truncating %s [" POINTER_PRINTF_FORMAT "] under pd2ud node %s [" POINTER_PRINTF_FORMAT "]\n",
         s->optDetailString(), child->getOpCode().getName(), child, node->getOpCode().getName(),node))
      {
      child = node->setChild(0, s->replaceNodeWithChild(child, child->getFirstChild(), s->_curTree, block, false)); // correctBCDPrecision=false because nodePrec==childPrec
      }

   TR::Node * result = NULL;
   if (result = s->unaryCancelOutWithChild(node, node->getFirstChild(), s->_curTree, TR::ud2pd))
      return result;

   child = node->setChild(0,removeOperandWidening(child, node, block, s));

   if (node->getFirstChild()->getOpCodeValue() == TR::pdclean || node->getFirstChild()->getOpCodeValue() == TR::pdSetSign)
      {
      node->setChild(0,s->replaceNodeWithChild(node->getFirstChild(), node->getFirstChild()->getFirstChild(), s->_curTree, block));
      return s->simplify(node, block);
      }

   child = node->getFirstChild();
   if (child->getReferenceCount() == 1 &&
       (child->getOpCode().isSetSign() || TR::ILOpCode::setSignVersionOfOpCode(child->getOpCodeValue()) != TR::BadILOp))
      {
      TR::Node *newSetSignValue = TR::Node::iconst(child, TR::DataType::getIgnoredSignCode());
      if (child->getOpCode().isSetSign())
         {
         int32_t childSetSignIndex = TR::ILOpCode::getSetSignValueIndex(child->getOpCodeValue());
         TR::Node *oldSetSignValue = child->getChild(childSetSignIndex);
         if (oldSetSignValue->getOpCode().isLoadConst() &&
             performTransformation(s->comp(), "%sReplace %s [" POINTER_PRINTF_FORMAT "] dominated %s [" POINTER_PRINTF_FORMAT "] with setSignValueNode [" POINTER_PRINTF_FORMAT "] and value (0x%x) with new ",
               s->optDetailString(),node->getOpCode().getName(),node,child->getOpCode().getName(),child,oldSetSignValue,oldSetSignValue->get32bitIntegralValue()))
            {
            newSetSignValue->incReferenceCount();
            oldSetSignValue->recursivelyDecReferenceCount();
            child->setChild(childSetSignIndex, newSetSignValue);
            child->resetSignState();
            dumpOptDetails(s->comp(), "setSignValueNode [" POINTER_PRINTF_FORMAT "] and ignored value (%d)\n", newSetSignValue, TR::DataType::getIgnoredSignCode());
            }
         }
      else if (performTransformation(s->comp(), "%sReplace %s [" POINTER_PRINTF_FORMAT "] dominated %s [" POINTER_PRINTF_FORMAT "] with new ",
                  s->optDetailString(),node->getOpCode().getName(),node,child->getOpCode().getName(),child))

         {
         TR::Node *newNode = NULL;
         TR::ILOpCodes setSignOp = TR::ILOpCode::setSignVersionOfOpCode(child->getOpCodeValue());
         int32_t newSetSignIndex = TR::ILOpCode::getSetSignValueIndex(setSignOp);
         if (newSetSignIndex == 1) // child 2
            {
            if (child->getOpCode().isConversion() &&
                child->getFirstChild()->getType().isUnicodeSeparateSign() &&
                child->getSecondChild()->getDataType() == TR::Address)
               {
               // Transform:
               //
               //     pd2ud - node
               //       udsx2pd - child
               //          x
               //          signAddrForCompareAndSet (could be '-' or '+')
               //
               // to:
               //
               //    p2dud
               //       udsx2pdSetSign - newNode
               //          x
               //          iconst TR::DataType::getIgnoredSignCode()
               //
               child->getSecondChild()->recursivelyDecReferenceCount();
               newNode = TR::Node::create(setSignOp, 2,
                                         child->getFirstChild(),
                                         newSetSignValue);
               }
            else if (child->getNumChildren() == 1)
               {
               // Transform:
               //
               //    pd2ud - node
               //       zdsts2pd - child
               //          x
               //
               // to:
               //
               //    pd2ud
               //       zdsts2pdSetSign - newNode
               //          x
               //          iconst TR::DataType::getIgnoredSignCode()
               //

               newNode = TR::Node::create(setSignOp, 2,
                                         child->getFirstChild(),
                                         newSetSignValue);
               }
            else
               {
               TR_ASSERT(false,"unexpected parent/child combination in pd2udSimplifier for newSetSignIndex == 1\n");
               }
            }
         else if (newSetSignIndex == 2) // child 3
            {
            // Transform:
            //
            //  pd2ud - node
            //     pdshl - child
            //        x
            //        iconst shift
            // to:
            //
            //  pd2ud - node
            //     pdshlSetSign - newNode
            //        x
            //        iconst shift
            //        iconst TR::DataType::getIgnoredSignCode()
            //
            TR_ASSERT(child->getNumChildren() == 2,"expecting two children on the child node (%p) and not %d children\n",child,child->getNumChildren());
            newNode = TR::Node::create(setSignOp, 3,
                                      child->getFirstChild(),
                                      child->getSecondChild(),             // shiftAmount
                                      newSetSignValue);
            }
         else if (newSetSignIndex == 3) // child 4
            {
            // Transform:
            //
            //  pd2ud - node
            //     pdshr - child
            //        x
            //        iconst shift
            //        iconst round=0
            // to:
            //
            //  pd2ud - node
            //     pdshrSetSign - newNode
            //        x
            //        iconst shift
            //        iconst round=0
            //        iconst TR::DataType::getIgnoredSignCode()
            //
            TR_ASSERT(child->getNumChildren() == 3,"expecting three children on the child node (%p) and not %d children\n",child,child->getNumChildren());
            newNode = TR::Node::create(setSignOp, 4,
                                      child->getFirstChild(),
                                      child->getSecondChild(),             // shiftAmount
                                      child->getThirdChild(),              // roundAmount
                                      newSetSignValue);
            }
         else
            {
            TR_ASSERT(false,"unexpected newSetSignIndex of %d\n",newSetSignIndex);
            }
         if (newNode)
            {
            //printf("changing %p pdshx %p to new setsign %p\n",node,child,newNode);
            dumpOptDetails(s->comp(), " %s [" POINTER_PRINTF_FORMAT "] to ignored value (%d) [" POINTER_PRINTF_FORMAT "]\n", newNode->getOpCode().getName(),newNode,TR::DataType::getIgnoredSignCode());
            newNode->incReferenceCount();
            newNode->setDecimalPrecision(child->getDecimalPrecision());
            // All the children up to but not including the setSignValue on newNode were existing nodes and were therefore incremented one extra time each when creating newNode above
            for (int32_t i=0; i < newNode->getNumChildren()-1; i++)
               newNode->getChild(i)->decReferenceCount();
            stopUsingSingleNode(child, true, s); // removePadding=true
            node->setChild(0, s->simplify(newNode,block));
            child = newNode;
            }
         }
      }
   return node;
   }

TR::Node *ud2pdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   if (!node->hasKnownOrAssumedSignCode())
      {
      TR_RawBCDSignCode sign = s->comp()->cg()->alwaysGeneratedSign(node);
      if (sign != raw_bcd_sign_unknown &&
          performTransformation(s->comp(), "%sSetting known sign 0x%x on %s [" POINTER_PRINTF_FORMAT "] from alwaysGeneratedSign\n",
            s->optDetailString(),TR::DataType::getValue(sign),node->getOpCode().getName(),node))
         {
         node->setKnownSignCode(sign);
         }
      }

   TR::Node * result = NULL;
   if (result = s->unaryCancelOutWithChild(node, node->getFirstChild(), s->_curTree, TR::pd2ud))
      return result;
   return node;
   }

// Handles udst2pd, udsl2pd
TR::Node *udsx2pdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   propagateSignStateUnaryConversion(node, block, s);

   TR::DataType sourceDataType = TR::NoType;
   TR::DataType targetDataType = TR::NoType;
   if (decodeConversionOpcode(node->getOpCode(), node->getDataType(), sourceDataType, targetDataType))
      {
      TR::ILOpCodes inverseOp = TR::ILOpCode::getProperConversion(targetDataType, sourceDataType, false /* !wantZeroExtension */); // inverse conversion to what we are given

      TR::Node * result = NULL;
      if (result = s->unaryCancelOutWithChild(node, node->getFirstChild(), s->_curTree, inverseOp))
         return result;
      }
   return node;
   }

// Also handles pd2udst
TR::Node *pd2udslSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   node->setChild(0,removeShiftTruncationForConversionParent(node, block, s));
   node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));

   if (node->getFirstChild()->getOpCodeValue() == TR::pdSetSign)
      {
      TR::Node *newNode = foldSetSignIntoNode(node->getFirstChild(), true /* setSignIsTheChild */, node, true /* removeSetSign */, block, s);
      if (newNode != node)
         return newNode;
      }

   if (node->getFirstChild()->getOpCode().isSetSign())
      {
      TR::Node *newNode = foldAndReplaceDominatedSetSign(node->getFirstChild(), true /* setSignIsTheChild */, node, block, s);
      if (newNode != node)
         return newNode;
      }

   TR::Node *newNode = createSetSignForKnownSignChild(node, block, s);
   if (newNode != node)
      return newNode;

   newNode = foldSetSignFromGrandChild(node, block, s);
   if (newNode != node)
      return newNode;

   return node;
   }

TR::Node *zdsle2pdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   node->setChild(0,removeShiftTruncationForConversionParent(node, block, s));
   node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));

   return node;
   }

// Also handles udst2ud
TR::Node *udsl2udSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   node->setChild(0,removeShiftTruncationForConversionParent(node, block, s));
   node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));

   return node;
   }

// Also handles zdsts2zd
TR::Node *zdsls2zdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   node->setChild(0,removeShiftTruncationForConversionParent(node, block, s));
   node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));

   return node;
   }

//---------------------------------------------------------------------
// Packed decimal to zoned decimal sign leading separate
//
// also handles pd2zdsts
TR::Node *pd2zdslsSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   TR_ASSERT(node->getOpCodeValue() == TR::pd2zdsls || node->getOpCodeValue() == TR::pd2zdsts,"pd2zdslsSimplifier only valid for TR::pd2zdsls/TR::pd2zdsts nodes\n");
   simplifyChildren(node, block, s);

   propagateSignStateUnaryConversion(node, block, s);

   TR::Node *child = node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));

   TR::DataType sourceDataType = TR::NoType;
   TR::DataType targetDataType = TR::NoType;
   if (decodeConversionOpcode(node->getOpCode(), node->getDataType(), sourceDataType, targetDataType))
      {
      TR::ILOpCodes inverseOp = TR::ILOpCode::getProperConversion(targetDataType, sourceDataType, false /* !wantZeroExtension */); // inverse conversion to what we are given

      TR::Node * result = NULL;
      if ((node->getDecimalPrecision() >= child->getDecimalPrecision()) &&
          (result = s->unaryCancelOutWithChild(node, child, s->_curTree, inverseOp)))
         return result;
      }

   if (node->getFirstChild()->getOpCodeValue() == TR::pdSetSign)
      {
      TR::Node *newNode = foldSetSignIntoNode(node->getFirstChild(), true /* setSignIsTheChild */, node, true /* removeSetSign */, block, s);
      if (newNode != node)
         return newNode;
      }
   if (node->getFirstChild()->getOpCode().isSetSign())
      {
      TR::Node *newNode = foldAndReplaceDominatedSetSign(node->getFirstChild(), true /* setSignIsTheChild */, node, block, s);
      if (newNode != node)
         return newNode;
      }

   if (node->getFirstChild()->getOpCodeValue() == TR::zd2pd)
      {
      TR::Node *grandChild = s->unaryCancelOutWithChild(node, node->getFirstChild(), s->_curTree, TR::zd2pd); // unaryCancelOutWithChild takes care of the details of removing the pd2zdsls/zd2pd
      if (grandChild)
         {
         TR::Node *newNode = TR::Node::create(node->getDataType() == TR::ZonedDecimalSignTrailingSeparate ? TR::zd2zdsts : TR::zd2zdsls, 1, grandChild);
         grandChild->decReferenceCount();
         newNode->incReferenceCount();
         newNode->setDecimalPrecision(node->getDecimalPrecision());
         dumpOptDetails(s->comp(), "%screated new %s [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), newNode->getOpCode().getName(), newNode);
         return newNode;
         }
      }

   child = node->setChild(0, flipCleanAndShift(node->getFirstChild(), block, s));

   child = node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));

   return node;
   }

// Also handles pd2zdstsSetSign
TR::Node *pd2zdslsSetSignSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   return node;
   }

//---------------------------------------------------------------------
// Zoned decimal to zoned decimal sign leading separate
//
// Also handles zd2zdsts
TR::Node *zd2zdslsSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   TR_ASSERT(node->getOpCodeValue() == TR::zd2zdsls || node->getOpCodeValue() == TR::zd2zdsts,"zd2zdslsSimplifier only valid for TR::zd2zdsls/TR::zd2zdsts nodes\n");
   simplifyChildren(node, block, s);

   propagateSignStateUnaryConversion(node, block, s);

   TR::Node *child = node->getFirstChild();

   // do not remove widening on the leading sign case to avoid having to set the sign first on the
   // narrower zd2zdsls conversion and then possibly move it again on a later widening.
   if (node->getType().isTrailingSeparateSign())
      child = node->setChild(0,removeOperandWidening(child, node, block, s));

   TR::Node *newNode = foldSetSignFromGrandChild(node, block, s);
   if (newNode != node)
      return newNode;

   TR::DataType sourceDataType = TR::NoType;
   TR::DataType targetDataType = TR::NoType;
   if (decodeConversionOpcode(node->getOpCode(), node->getDataType(), sourceDataType, targetDataType))
      {
      TR::ILOpCodes inverseOp = TR::ILOpCode::getProperConversion(targetDataType, sourceDataType, false /* !wantZeroExtension */); // inverse conversion to what we are given

      TR::Node * result = NULL;
      if ((node->getDecimalPrecision() == child->getDecimalPrecision()) &&
          (result = s->unaryCancelOutWithChild(node, child, s->_curTree, inverseOp)))
         return result;
      }
   return node;
   }

//---------------------------------------------------------------------
// Zoned decimal sign leading separate to packed decimal
//
// Also handles zdsts2pd
TR::Node *zdsls2pdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   propagateSignStateUnaryConversion(node, block, s);

   return node;
   }

//---------------------------------------------------------------------
// Zoned decimal sign leading embedded to zoned decimal
//
TR::Node *zdsle2zdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   propagateSignStateUnaryConversion(node, block, s);
   TR::Node *child = node->getFirstChild();

   TR::Node * result = NULL;
   if ((node->getDecimalPrecision() == child->getDecimalPrecision()) &&
       (result = s->unaryCancelOutWithChild(node, child, s->_curTree, TR::zd2zdsle)))
      return result;

   return node;
   }

//---------------------------------------------------------------------
// Zoned decimal to packed decimal
//
TR::Node *zd2pdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   propagateSignStateUnaryConversion(node, block, s);

   TR::Node * result = NULL;
   if (result = s->unaryCancelOutWithChild(node, firstChild, s->_curTree, TR::pd2zd))
      return result;

   if (firstChild->getOpCodeValue() == TR::zdsle2zd &&
       firstChild->getReferenceCount() == 1 &&
       node->getDecimalPrecision() < firstChild->getDecimalPrecision() &&
       performTransformation(s->comp(), "%sReduce zdsle2zd child [" POINTER_PRINTF_FORMAT "] precision to %d due to truncating zd2pd [" POINTER_PRINTF_FORMAT "]\n",s->optDetailString(),firstChild,node->getDecimalPrecision(),node))
      {
      firstChild->setDecimalPrecision(node->getDecimalPrecision());
      }

   firstChild = node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));

   return node;
   }

TR::Node *pd2zdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (node->getDecimalPrecision() == firstChild->getDecimalPrecision() &&
       firstChild->isSimpleTruncation() &&
       performTransformation(s->comp(), "%sRemove simple truncating %s [" POINTER_PRINTF_FORMAT "] under pd2zd node %s [" POINTER_PRINTF_FORMAT "]\n",
         s->optDetailString(), firstChild->getOpCode().getName(), firstChild, node->getOpCode().getName(),node))
      {
      firstChild = node->setChild(0, s->replaceNodeWithChild(firstChild, firstChild->getFirstChild(), s->_curTree, block, false)); // correctBCDPrecision=false because nodePrec==childPrec
      }

   propagateSignStateUnaryConversion(node, block, s);

   TR::Node * result = NULL;
   if (result = s->unaryCancelOutWithChild(node, firstChild, s->_curTree, TR::zd2pd))
      return result;

   TR::Node *child = node->setChild(0, flipCleanAndShift(node->getFirstChild(), block, s));

   if ((child->getReferenceCount() == 1 && node->getReferenceCount() == 1) &&
       (child->getOpCodeValue() == TR::zdsle2pd ||
        child->getOpCodeValue() == TR::zdsls2pd ||
        child->getOpCodeValue() == TR::zdsts2pd))
      {
      if ((node->getDecimalPrecision() == child->getDecimalPrecision()) &&
           performTransformation(s->comp(), "%sFold %s [" POINTER_PRINTF_FORMAT "] into child %s [" POINTER_PRINTF_FORMAT "] and create new\n",
            s->optDetailString(), node->getOpCode().getName(), node, child->getOpCode().getName(), child))
         {
         // Transform:
         // pd2zd
         //   zdsls2pdSetSign
         //     x
         //     sign
         // To:
         //   zdsls2zdSetSign
         //     x
         //     sign
         bool childIsSetSign = child->getOpCode().isSetSign();
         TR::ILOpCodes childOpValueForLookup = childIsSetSign ? TR::ILOpCode::reverseSetSignOpCode(child->getOpCodeValue()) : child->getOpCodeValue();
         TR::ILOpCode childOpForLookup;
         childOpForLookup.setOpCodeValue(childOpValueForLookup);
         TR::DataType sourceDataType = TR::NoType;
         TR::DataType targetDataType = TR::NoType;
         if (childOpValueForLookup != TR::BadILOp && decodeConversionOpcode(childOpForLookup, child->getDataType(), sourceDataType, targetDataType))
            {
            TR::ILOpCodes newConvOp = TR::ILOpCode::getProperConversion(sourceDataType, TR::ZonedDecimal, false /* !wantZeroExtension */);
            TR::Node *newNode = NULL;
            if (newConvOp != TR::BadILOp)
               {
               TR::ILOpCodes newSetSignOp = TR::ILOpCode::setSignVersionOfOpCode(newConvOp);
               if (childIsSetSign)
                  {
                  if (newSetSignOp != TR::BadILOp)
                     {
                     newNode = TR::Node::create(newSetSignOp, 2,
                                               child->getFirstChild(),
                                               child->getSecondChild());
                     child->getSecondChild()->decReferenceCount();
                     }
                  }
               else
                  {
                  newNode = TR::Node::create(newConvOp, 1, child->getFirstChild());
                  }
               }
            if (newNode)
               {
               child->getFirstChild()->decReferenceCount();
               dumpOptDetails(s->comp(), "%s [" POINTER_PRINTF_FORMAT "]\n", newNode->getOpCode().getName(), newNode);
               newNode->incReferenceCount();
               newNode->setDecimalPrecision(node->getDecimalPrecision());
               stopUsingSingleNode(node, true, s); // removePadding=true
               stopUsingSingleNode(child, true, s); // removePadding=true
               return newNode;
               }
            }
         else
            {
            TR_ASSERT(false,"could not find conversion in pd2zdSimplifier\n");
            }
         }
      }

   child = node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));

   if (!s->comp()->getOption(TR_DisableZonedToDFPReduction) &&
       s->cg()->supportsZonedDFPConversions() &&
       child->getOpCodeValue() == TR::pdclean &&
       child->getFirstChild()->getOpCode().isConversion() &&
       child->getFirstChild()->getFirstChild()->getType().isDFP() &&
       child->getFirstChild()->getDecimalFraction() == 0 &&
       !child->getFirstChild()->useCallForFloatToFixedConversion())
      {
      // pd2zd
      //    pdclean
      //       de2pd
      //          deX
      // to:
      // de2zdClean
      //    deX
      //
      TR::ILOpCodes convOp = TR::ILOpCode::dfpToZonedCleanOp(child->getFirstChild()->getFirstChild()->getDataType(), node->getDataType());
      if (convOp != TR::BadILOp &&
          !node->hasIntermediateTruncation() &&
          performTransformation(s->comp(), "%sFold %s (0x%p) and dfpToZoned cleaning child %s (0x%p) to ",s->optDetailString(),
            node->getOpCode().getName(),node,child->getOpCode().getName(),child))
         {
         int32_t fraction = child->getFirstChild()->getDecimalFraction();
         TR::ILOpCode newOp;
         newOp.setOpCodeValue(convOp);
         dumpOptDetails(s->comp(), "%s (0x%p)\n",newOp.getName(),node);
         removePaddingNode(node, s);
         TR::Node::recreate(node, convOp);    // pd2zd -> de2zdClean (momentarily type incorrect but not exposed outside this simplifier));
         node->setFlags(0);
         //dumpOptDetails(s->comp(), "%s\n",node->getOpCode().getName());
         // pd2zd (de2zdClean)
         //    pdclean
         //       de2pd
         //          deX
         // to
         // pd2zd (de2zdClean)
         //    de2pd
         //       deX
         // correctBCDPrecision=false trunc checks already done and top level prec is being maintained (false is needed so pdclean doesn't just get changed to a pdModPrec)
         node->setChild(0, s->replaceNodeWithChild(node->getFirstChild(), node->getFirstChild()->getFirstChild(), s->_curTree, block, false));
         // pd2zd  (de2zdClean)
         //    de2pd
         //      deX
         // to
         // pd2zd (de2zdClean)
         //    deX
         // correctBCDPrecision=false trunc checks already done and top level prec is being maintained (false is needed so de2pd doesn't just get changed to a pdModPrec)
         node->setChild(0, s->replaceNodeWithChild(node->getFirstChild(), node->getFirstChild()->getFirstChild(), s->_curTree, block, false));
         node->setHasKnownCleanSign(true);
         node->setUseCallForFloatToFixedConversion(false);
         node->setDecimalFraction(fraction);
         if (child->getFirstChild()->hasSourcePrecision())
            node->setSourcePrecision(child->getFirstChild()->getSourcePrecision());
         return node;
         }
      }

   if (!s->comp()->getOption(TR_DisableZonedToDFPReduction) &&
       s->cg()->supportsZonedDFPConversions() &&
       child->getOpCode().isConversion() &&
       child->getFirstChild()->getType().isDFP() &&
       child->getDecimalFraction() == 0 &&
       !child->useCallForFloatToFixedConversion())
      {
      // pd2zd
      //    de2pd/de2pdSetSign/de2pdClean
      //       deX
      // to:
      // de2zd/de2zdSetSign/de2zdClean
      //    deX
      //
      bool isSetSign = child->getOpCode().isSetSignOnNode();
      bool isClean = (child->getOpCodeValue() == TR::ILOpCode::dfpToPackedCleanOp(child->getFirstChild()->getDataType(), child->getDataType()));
      TR_RawBCDSignCode sign = node->getKnownOrAssumedSignCode();

      TR::ILOpCodes convOp = TR::ILOpCode::dfpToZonedOp(child->getFirstChild()->getDataType(), node->getDataType());
      if (isSetSign)
         convOp = TR::ILOpCode::dfpToZonedSetSignOp(child->getFirstChild()->getDataType(), node->getDataType());
      if (isClean)
         convOp = TR::ILOpCode::dfpToZonedCleanOp(child->getFirstChild()->getDataType(), node->getDataType());

      if (!isSetSign || (sign != raw_bcd_sign_unknown))
         {
         // could generalize the <= prec check below but would have to start looking at DFP max precision in replaceNodeWithChild
         // e.g. the de2pd could be truncating if the result pd precision is less than the max possible from a de
         // but for now just handle the common case of <= (this is safe as a truncation on top of another truncation is ok)
         if (convOp != TR::BadILOp &&
             node->getDecimalPrecision() <= child->getDecimalPrecision() && // don't lose a truncation
             performTransformation(s->comp(), "%sFold %s (0x%p) and dfpToPacked child %s (0x%p) to ",s->optDetailString(),
               node->getOpCode().getName(),node,child->getOpCode().getName(),child))
            {
            int32_t fraction = child->getDecimalFraction();
            TR::ILOpCode newOp;
            newOp.setOpCodeValue(convOp);
            dumpOptDetails(s->comp(), "%s (0x%p)\n",newOp.getName(),node);
            removePaddingNode(node, s);
            TR::Node::recreate(node, convOp);    // pd2zd -> de2zd (momentarily type incorrect but not exposed outside this simplifier)
            node->setFlags(0);
            if (isSetSign)
               {
               node->resetSignState();
               node->setSetSign(sign);
               }
            // pd2zd (de2zd/de2zdSetSign)
            //    de2pd/de2pdSetSign
            //       deX
            // to
            // pd2zd (de2zd/de2zdSetSign)
            //    deX
            // correctBCDPrecision=false trunc checks already done and top level prec is being maintained (false is needed so pdclean doesn't just get changed to a pdModPrec)
            node->setChild(0, s->replaceNodeWithChild(node->getFirstChild(), node->getFirstChild()->getFirstChild(), s->_curTree, block, false));
            node->setUseCallForFloatToFixedConversion(false);
            node->setDecimalFraction(fraction);
            if (child->hasSourcePrecision())
               node->setSourcePrecision(child->getSourcePrecision());
            return node;
            }
         }
      }
   return node;
   }

// Handles zd2df, zd2dd, zd2de
TR::Node *zd2dfSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   bool isAbs = false;
   if (node->getOpCodeValue() == TR::zd2dfAbs || node->getOpCodeValue() == TR::zd2ddAbs || node->getOpCodeValue() == TR::zd2deAbs)
      isAbs = true;

   TR::Node *child = node->getFirstChild();

   // zd2dd over dd2zd: both can be cancelled
   if (!isAbs)
      {
      TR::Node *result = s->unaryCancelOutWithChild(node, child, s->_curTree, TR::ILOpCode::getProperConversion(node->getDataType(), child->getDataType(), false));
      if (result != NULL)
         return result;
      }

   return node;
   }

TR::Node *df2zdSimplifier(TR::Node *node, TR::Block *block, TR::Simplifier *s)
   {
   simplifyChildren(node, block, s);

   // dd2zd over zd2dd: both can be cancelled
   TR::Node *child = node->getFirstChild();
   TR::Node *result = s->unaryCancelOutWithChild(node, child, s->_curTree, TR::ILOpCode::getProperConversion(node->getDataType(), child->getDataType(), false));
   if (result != NULL)
      return result;

   return node;
   }

// Handles df2zdSetSign, dd2zdSetSign, de2zdSetSign
TR::Node *df2zdSetSignSimplifier(TR::Node *node, TR::Block *block, TR::Simplifier *s)
   {
   simplifyChildren(node, block, s);

   TR::Node *child = node->getFirstChild();
   child = node->setChild(0,removeUnnecessaryDFPClean(child, node, block, s));

   return node;
   }

// Convert a pdshX/pdshXSetSign/pdSetSign -> dd2pd -> ddX to dd2pd/dd2pdSetSign -> [ddshX] -> ddX.
// With other simplifications, this may prevent a conversion from DFP to packed after performing math in DFP.
// DFP is often used for trees storing to and rooted in zoned, as the conversion is faster than going to packed.
static TR::Node *lowerPackedShiftOrSetSignBelowDFPConv(TR::Node *node, TR::Block *block, TR::Simplifier *s)
   {
   TR::ILOpCodes op = node->getOpCodeValue();
   TR_ASSERT(op == TR::pdshl || op == TR::pdshr || op == TR::pdshlSetSign || op == TR::pdshrSetSign || op == TR::pdSetSign,
             "Node %s [0x%x] isn't a packed shift/setSign in lowerPackedShiftOrSetSignBelowDFPConv\n",
             node->getOpCode().getName(), node);

   TR::Node *firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isConversion() &&
       firstChild->getFirstChild()->getType().isDFP() &&
       !node->hasIntermediateTruncation())
      {
      TR::Node *roundNode = NULL;
      TR::Node *signNode = NULL;
      bool isSetSign = node->getOpCode().isSetSign();
      bool isShift = node->getOpCode().isShift();
      bool hasRoundNode = node->getOpCode().isRightShift();

      if (hasRoundNode)
         roundNode = node->getChild(2);
      if (isSetSign)
         signNode = node->getChild(TR::ILOpCode::getSetSignValueIndex(node->getOpCodeValue()));

      if (hasRoundNode && !roundNode->getOpCode().isLoadConst())
         return node;

      if (isSetSign && !signNode->getOpCode().isLoadConst())
         return node;

      // If the shift node widens, moving it below the conversion might require it to be a different DFP type than dfpNode.
      // Find the DFP types needed to represent the shift and conv nodes, and if they're not the same, insert a dX2dY node.
      // A shift could also widen and then truncate in one op, so we want to use the DFP type that fits the max precision
      // out of the node's precision, the child's precision, and the shifted amount, to ensure that truncation only
      // happens on the final DFP-to-packed conversion.
      TR::Node *dfpSourceNode = firstChild->getFirstChild();
      TR::DataType convType = dfpSourceNode->getDataType();
      TR::DataType resultType = convType;

      TR_ASSERT(convType.canGetMaxPrecisionFromType(),"cannot get max precision from type %s on node %s %p\n",
         TR::DataType::getName(convType),dfpSourceNode->getOpCode().getName(),dfpSourceNode);
      int32_t dfpSourcePrecision = 0;
      if (firstChild->hasSourcePrecision())
         dfpSourcePrecision = firstChild->getSourcePrecision();
      else
         dfpSourcePrecision = convType.getMaxPrecisionFromType();

      int32_t maxDFPPrecision = std::max<int32_t>(node->getDecimalPrecision(), dfpSourcePrecision);
      int32_t round = hasRoundNode ? roundNode->get32bitIntegralValue() : 0;
      int32_t roundAmountBump = (round == 5) ? 1 : 0;
      if (isShift)
         maxDFPPrecision = std::max(firstChild->getDecimalPrecision() + node->getDecimalAdjust() + roundAmountBump, maxDFPPrecision);

      if (maxDFPPrecision > TR::DataType::getMaxExtendedDFPPrecision())
         {
         if (s->trace() || s->comp()->cg()->traceBCDCodeGen())
            traceMsg(s->comp(),"z^z : maxDFPPrecision %d > TR::DataType::getMaxExtendedDFPPrecision() %d in attempting to create a dfp shift from %s (%p) at line_no=%d (offset=%06X)\n",
               maxDFPPrecision,TR::DataType::getMaxExtendedDFPPrecision(),node->getOpCode().getName(),node,s->comp()->getLineNumber(node),s->comp()->getLineNumber(node));
         return node;
         }

      resultType = TR::DataType::getDFPTypeFromPrecision(maxDFPPrecision);
      if (resultType == TR::DecimalFloat && convType != TR::DecimalFloat)
         resultType = TR::DecimalDouble;

      int32_t sign = isSetSign ? signNode->get32bitIntegralValue() : 0xc; // Default of + is safe here; sign will only be set if isSetSign is true

      if ((round == 0 || round == 5) &&
          resultType != TR::NoType && convType != TR::NoType &&
          (!isSetSign || TR::DataType::isSupportedRawSign(sign)) &&
          performTransformation(s->comp(), "%sMoving %s [" POINTER_PRINTF_FORMAT "] below child %s [" POINTER_PRINTF_FORMAT "]\n",
                                s->optDetailString(),node->getOpCode().getName(),node,firstChild->getOpCode().getName(),firstChild))
         {
         TR::Node *shift = node, *newShift = NULL, *conv = firstChild, *newConv, *dfpNode = conv->getFirstChild(), *shiftAmountNode = node->getSecondChild();

         // Without rounding, pdshX[SetSign] becomes ddshX
         if (isShift)
            {
            if (round == 0)
               {
               newShift = TR::Node::create(node, TR::ILOpCode::dfpOpForBCDOp(node->getOpCode().isRightShift() ? TR::pdshr : TR::pdshl, resultType), 2);
               }
            else if (round == 5)
               {
               // With rounding, pdshX[SetSign] becomes ddshXRounded
               newShift = TR::Node::create(node, TR::ILOpCode::shiftRightRoundedOpCode(resultType), 3);

               // Better codegen by using a ddconst and converting to de instead of using a deconst
               TR::DataType roundType = resultType;
               if (roundType == TR::DecimalLongDouble)
                  roundType = TR::DecimalDouble;

               TR::Node *roundNode = TR::Node::create(node, TR::ILOpCode::constOpCode(roundType));
               if (roundType == TR::DecimalFloat)
                  {
                  char numbers[4] = { 0x22, 0x50, 0x00, 0x01 };
                  roundNode->setFloat(*(float *)numbers);
                  }
               else if (roundType == TR::DecimalDouble)
                  {
                  char numbers[8] = { 0x22, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
                  roundNode->setDouble(*(double *)numbers);
                  }

               if (roundType != resultType)
                  {
                  TR::Node *t = TR::Node::create(node, TR::ILOpCode::getProperConversion(roundType, resultType, false), 1);
                  t->setAndIncChild(0, roundNode);
                  dumpOptDetails(s->comp(),"Created conv %s [" POINTER_PRINTF_FORMAT "] on child round %s [" POINTER_PRINTF_FORMAT "]\n",
                                 t->getOpCode().getName(),t, roundNode->getOpCode().getName(),roundNode);
                  roundNode = t;
                  }

               newShift->setAndIncChild(2, roundNode);
               dumpOptDetails(s->comp(),"Created dfp shift %s [" POINTER_PRINTF_FORMAT "] and child round %s [" POINTER_PRINTF_FORMAT "]\n",
                              newShift->getOpCode().getName(),newShift, roundNode->getOpCode().getName(),roundNode);
               }

            if (resultType != convType)
               {
               TR::Node *dfpConv = TR::Node::create(node, TR::ILOpCode::getProperConversion(convType, resultType, false), 1);
               dfpConv->setAndIncChild(0, dfpNode);
               newShift->setAndIncChild(0, dfpConv);
               dumpOptDetails(s->comp(),"%sCreated node %s [" POINTER_PRINTF_FORMAT "] on top of %s [" POINTER_PRINTF_FORMAT "] to preserve shift precision\n",s->optDetailString(),
                  dfpConv->getOpCode().getName(),dfpConv,dfpNode->getOpCode().getName(),dfpNode);
               }
            else
               newShift->setAndIncChild(0, dfpNode); // dd2pd -> ddshX -> ddX

            newShift->setAndIncChild(1, shiftAmountNode);

            dumpOptDetails(s->comp(),"%screate newShift %s [" POINTER_PRINTF_FORMAT "] of %s [" POINTER_PRINTF_FORMAT "] by %s [" POINTER_PRINTF_FORMAT "]\n",s->optDetailString(),
               newShift->getOpCode().getName(),newShift,
               newShift->getFirstChild()->getOpCode().getName(),newShift->getFirstChild(),
               newShift->getSecondChild()->getOpCode().getName(),newShift->getSecondChild());
            }
         else
            {
            // Just a setsign: de2pd -> dd2de -> ddX
            if (resultType != convType)
               {
               TR::Node *dfpConv = TR::Node::create(node, TR::ILOpCode::getProperConversion(convType, resultType, false), 1);
               dfpConv->setAndIncChild(0, dfpNode);
               newShift = dfpConv;
               dumpOptDetails(s->comp(),"%sCreated node %s [" POINTER_PRINTF_FORMAT "] to preserve shift precision\n",s->optDetailString(),dfpConv->getOpCode().getName(),dfpConv);
               }
            else
               newShift = dfpNode; // dd2pd -> ddX
            }

         if (isSetSign)
            {
            newConv = TR::Node::create(node, TR::ILOpCode::dfpToPackedSetSignOp(resultType, TR::PackedDecimal), 1);
            newConv->setSetSign(TR::DataType::getSupportedRawSign(sign));
            }
         else
            {
            newConv = TR::Node::create(node, TR::ILOpCode::getProperConversion(resultType, TR::PackedDecimal, false), 1);
            }
         newConv->setAndIncChild(0, newShift);
         newConv->setDecimalPrecision(shift->getDecimalPrecision());

         // srcPrecision on the new conversion node needs to account for the fact that
         // shifting below the conversion increases the number of non-leading-zero digits eg.
         // pdshl <p=9, adj=4>        dd2pd <p=9,srcP=7>
         //   dd2pd <p=5,srcP=3>        ddshl <adj=4, p=3 implied>
         //     ddX <p=3 implied>         ddX <p=3 implied>
         //   iconst 4                    iconst 4
         if (conv->hasSourcePrecision() && conv->getSourcePrecision() + shift->getDecimalAdjust() + roundAmountBump > 0)
            newConv->setSourcePrecision(conv->getSourcePrecision() + shift->getDecimalAdjust() + roundAmountBump);
         dumpOptDetails(s->comp(),"%sReplacing %s [" POINTER_PRINTF_FORMAT "] and child %s [" POINTER_PRINTF_FORMAT "] with %s [" POINTER_PRINTF_FORMAT "]\n",s->optDetailString(),
                        shift->getOpCode().getName(),shift,conv->getOpCode().getName(),conv,
                        newConv->getOpCode().getName(),newConv);

         s->replaceNode(shift, newConv, s->_curTree, false);
         return newConv;
         }
      }
   return node;
   }

static TR::Node *reduceShiftRightOverShiftLeft(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   if (node->getOpCode().isPackedRightShift() &&
       node->getFirstChild()->getOpCode().isPackedLeftShift() &&
       !node->hasIntermediateTruncation())
      {
      TR::Node *child = node->getFirstChild();

      // overflow not supported
      if (child->getOpCodeValue() == TR::pdshlOverflow)
         return node;

      int32_t parentAdjust = node->getDecimalAdjust();
      int32_t childAdjust = child->getDecimalAdjust();
      int32_t combinedAdjust = parentAdjust + childAdjust;
      if (performTransformation(s->comp(), "%sFold rightShift-over-leftShift : %s by %d [0x%p] over %s by %d [0x%p] by setting parent adjust to %d and removing child\n",
            s->optDetailString(),node->getOpCode().getName(),parentAdjust,node,
            child->getOpCode().getName(),childAdjust,child,combinedAdjust))
         {
         bool parentIsSetSign = node->getOpCode().isSetSign();
         bool childIsSetSign = child->getOpCode().isSetSign();
         bool resultIsSetSign = parentIsSetSign || childIsSetSign;

         bool hasAdjust = combinedAdjust != 0;

         TR::Node *setSignNodeToUse = NULL;
         TR::Node *setSignValueNode = NULL;
         int32_t setSignValueIndex = -1;

         TR::ILOpCodes newOp = TR::BadILOp;
         uint32_t numChildren = 0;
         if (!hasAdjust)
            {
            newOp = resultIsSetSign ?
               TR::ILOpCode::setSignOpCode(node->getDataType()) :
               TR::ILOpCode::modifyPrecisionOpCode(node->getDataType());
            numChildren = resultIsSetSign ? 2 : 1;
            }
         else if (combinedAdjust < 0) // right shift
            {
            newOp = resultIsSetSign ? TR::pdshrSetSign : TR::pdshr;
            numChildren = resultIsSetSign ? 4 : 3;
            }
         else // combinedAdjust > 0 --> left shift
            {
            newOp = resultIsSetSign ? TR::pdshlSetSign : TR::pdshl;
            numChildren = resultIsSetSign ? 3 : 2;
            }
         TR_ASSERT(newOp != TR::BadILOp, "Folding result operation not found");

         bool shouldSkipReplacingSignChild = false;
         if (resultIsSetSign)
            {
            setSignNodeToUse = parentIsSetSign ? node : child;
            setSignValueNode = setSignNodeToUse->getSetSignValueNode();
            setSignValueIndex = TR::ILOpCode::getSetSignValueIndex(newOp);
            shouldSkipReplacingSignChild = setSignValueIndex >= node->getNumChildren();
            }

         // Remove excess children
         for (int32_t i = numChildren; i < node->getNumChildren(); i++)
            {
            TR::Node *removedChild = node->getChild(i);
            s->anchorNode(removedChild, s->_curTree);
            removedChild->recursivelyDecReferenceCount();
            }

         // Update opcode and move up grandchild
         node->setNumChildren(numChildren);
         node = TR::Node::recreate(node, newOp); // If necessary, this will resize the node extension to have enough room for all the children.
         node->setChild(0, s->replaceNodeWithChild(child, child->getFirstChild(), s->_curTree, block, false)); // correctBCDPrecision=false because truncChecks already done above

         if (hasAdjust)
            {
            node->setChild(1, s->replaceNode(node->getChild(1), TR::Node::iconst(node, abs(combinedAdjust)), s->_curTree));
            }
         if (resultIsSetSign)
            {
            // if the child at this index did not already exist, no need to call replace node on it
            if (shouldSkipReplacingSignChild)
               node->setAndIncChild(setSignValueIndex, setSignValueNode);
            else
               node->setChild(setSignValueIndex, s->replaceNode(node->getChild(setSignValueIndex), setSignValueNode, s->_curTree));
            }
         // Don't need to explicitly update the rounding child.
         // If node is still a shift right, the rounding child will still be correct and in the correct location.
         // If not, it was already removed or replaced above.
         }
      }
   return node;
   }

// combine pdshr->pdshr into a single pdshr
static TR::Node *reduceShiftRightOverShiftRight(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   TR::Node *firstChild = node->getFirstChild();

   // Only coalesce if first child is pdshr and all shift/round values are known.
   if (!(firstChild->getOpCodeValue() == TR::pdshr &&
         node->getSecondChild()->getOpCode().isLoadConst() &&
         node->getThirdChild()->getOpCode().isLoadConst() &&
         firstChild->getSecondChild()->getOpCode().isLoadConst() &&
         firstChild->getThirdChild()->getOpCode().isLoadConst()))
      return node;

   TR_ASSERT(node->getOpCodeValue() == TR::pdshr || node->getOpCodeValue() == TR::pdshrSetSign, "reduceShiftRightOverShiftRight should only be called on packed shift right node");

   int32_t nodeShift = node->getSecondChild()->get32bitIntegralValue();
   int32_t childShift = firstChild->getChild(1)->get32bitIntegralValue();

   int32_t nodeRound = node->getThirdChild()->get32bitIntegralValue();
   int32_t childRound = firstChild->getThirdChild()->get32bitIntegralValue();

   TR_ASSERT(nodeShift >= 0 && childShift >= 0,"node shift (%d) and child shift (%d) amounts should be >= 0\n",nodeShift,childShift);
   TR_ASSERT(nodeRound >= 0 && childRound >= 0,"node round (%d) and child round (%d) amounts should be >= 0\n",nodeRound,childRound);

   bool childIsTruncation = firstChild->getDecimalPrecision() < (firstChild->getFirstChild()->getDecimalPrecision() - childShift);
   bool nodeIsWidening = node->getDecimalPrecision() > (firstChild->getDecimalPrecision() - nodeShift);
   bool sourceIsTooLarge = firstChild->getFirstChild()->getDecimalPrecision() > TR::DataType::getMaxPackedDecimalPrecision() && nodeRound > 0;
/*
  dumpOptDetails(s->comp(), "\tchildPrec %d, childSrcPrec %d, childShift %d isTrunc=%s (childPrec %s (childSrcPrec-childShift))\n",
                 firstChild->getDecimalPrecision(),firstChild->getFirstChild()->getDecimalPrecision(),childShift,childIsTruncation?"yes":"no",childIsTruncation?"<":">=");
  dumpOptDetails(s->comp(), "\tnodePrec %d, nodeSrcPrec %d, nodeShift %d isWiden=%s (nodePrec %s (nodeSrcPrec-nodeShift))\n",
                 node->getDecimalPrecision(),firstChild->getDecimalPrecision(),nodeShift,nodeIsWidening?"yes":"no",nodeIsWidening?">":"<=");
  dumpOptDetails(s->comp(),"\tgrandChild %p prec %d sourceIsTooLarge=%s\n",firstChild->getFirstChild(),firstChild->getFirstChild()->getDecimalPrecision(),sourceIsTooLarge?"yes":"no");
*/
   bool foldingIsIllegal = (childIsTruncation && nodeIsWidening) || sourceIsTooLarge || childRound != 0;
   dumpOptDetails(s->comp(),"\tfoldingIsIllegal=%s\n",foldingIsIllegal?"yes":"no");
   if (foldingIsIllegal) return node;

   // An illegal case due to (childIsTruncation && nodeIsWidening) = true looks like:
   //    pdshr p=2,a=-2      // node
   //      pdshr p=3,a=-6    // child
   //        pdload p=10     // x
   //        iconst 6        // shift
   // childIsTruncation=true because the shifted precision of x is 10-6 = 4 but the child precision is 3 so 1 digit is being truncated.
   // nodeIsWidening=true because the shifted precision of child is 3-2 = 1 and this less than the node precision of 2
   // It is illegal to fold these two pdshr nodes into a single pdshr node because the truncated digit must be removed before it is examined by the widening
   // parent node.
   // If node precision was p=1 then it  nodeIsWidening=false and it would be legal to fold because the truncated digit would not be examined by the parent node.
   // If the parent node is rounding, we will be using an SRP, which is restricted to 32 bytes. If the parent node is not rounding, there is no length restriction.

   if (performTransformation(s->comp(), "%sFold non-truncating child pdshr [" POINTER_PRINTF_FORMAT "] into parent pdshr [" POINTER_PRINTF_FORMAT "] by setting nodeShift %d->%d and nodeRound %d->%d\n",
                             s->optDetailString(),firstChild,node,nodeShift,nodeShift+childShift,nodeRound,nodeRound))
      {
      node->setChild(0, s->replaceNodeWithChild(node->getChild(0), node->getChild(0)->getFirstChild(), s->_curTree, block, false)); // correctBCDPrecision=false because truncChecks already done above
      node->setChild(1, s->replaceNode(node->getChild(1), TR::Node::iconst(node, nodeShift+childShift), s->_curTree));
      // Don't have to replace rounding node. It will always be the same as the parent, because we do not merge if childRound != 0
      }

   return node;
   }

static TR::Node *reduceShiftLeftOverShiftRight(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   // lastRun note : doing on lastRun to give a better chance for setSigns to be folded into shifts so we can gen a pdclearSetSign here
   //                once the foldSetSign code is updated to handle setSignOnNode opcodes this can be removed
   //                Also, there are no pdclear/pdclearSetSign simplifiers yet so only transform once the tree is otherwise optimized
   if (node->getOpCode().isPackedLeftShift() &&
       s->getLastRun() && // see note above
       node->getFirstChild()->getOpCode().isPackedRightShift() &&
       node->getFirstChild()->getDecimalRound() == 0 &&
       !node->hasIntermediateTruncation())
      {
      TR::Node *child = node->getFirstChild();
      bool parentIsSetSign = node->getOpCode().isSetSign();
      bool childIsSetSign = child->getOpCode().isSetSign();
      int32_t parentAdjust = node->getDecimalAdjust();
      int32_t childAdjust = child->getDecimalAdjust();
      int32_t combinedAdjust = parentAdjust + childAdjust;
      TR::Node *parentSetSignValueNode = NULL;
      if (combinedAdjust == 0 &&
          (!parentIsSetSign || node->getSetSignValueNode()->getOpCode().isLoadConst()) &&
          (!childIsSetSign  || child->getSetSignValueNode()->getOpCode().isLoadConst()) &&
          performTransformation(s->comp(), "%sFold leftShift-over-rightShift : %s by %d [0x%p] over %s by %d [0x%p]\n",
            s->optDetailString(),node->getOpCode().getName(),parentAdjust,node,
            child->getOpCode().getName(),childAdjust,child))
         {
         int32_t digitsToClear = -childAdjust;
         int32_t leftMostNibbleToClear = digitsToClear;
         TR::Node *clearNode = NULL;
         if (parentIsSetSign || childIsSetSign)
            {
            TR::Node *setSignNodeToUse = parentIsSetSign ? node : child;
            TR::Node *setSignValueNode = setSignNodeToUse->getSetSignValueNode();
            int32_t sign = setSignValueNode->getSize() <= 4 ? setSignValueNode->get32bitIntegralValue() : 0;
            TR_RawBCDSignCode rawSign = TR::DataType::getSupportedRawSign(sign);
            if (rawSign != raw_bcd_sign_unknown)
               {
               bool genMaskAddress = false; // not currently taking advantage of this in the evaluator
               TR::Node *maskAddress = NULL;

               for (int32_t i = 1; i < node->getFirstChild()->getNumChildren(); i++) // node->getFirstChild()->getFirstChild() will remain anchored in the new clearNode
                  {
                  s->anchorNode(node->getFirstChild()->getChild(i), s->_curTree);
                  }
               if (maskAddress)
                  {
                  clearNode = TR::Node::create(TR::pdclearSetSign, 4,
                                              node->getFirstChild()->getFirstChild(),
                                              TR::Node::iconst(node, leftMostNibbleToClear),
                                              TR::Node::iconst(node, digitsToClear),
                                              maskAddress);
                  }
               else
                  {
                  clearNode = TR::Node::create(TR::pdclearSetSign, 3,
                                              node->getFirstChild()->getFirstChild(),
                                              TR::Node::iconst(node, leftMostNibbleToClear),
                                              TR::Node::iconst(node, digitsToClear));
                  }
               clearNode->setSetSign(rawSign);
               clearNode->setDecimalPrecision(node->getDecimalPrecision());
               dumpOptDetails(s->comp(),"\tcreate new %s [0x%p] :  leftDigit %d, digitsToClear %d, sign 0x%x, maskAddress 0x%p\n",
                  clearNode->getOpCode().getName(),clearNode,leftMostNibbleToClear,digitsToClear,(int32_t)sign,maskAddress);
               }
            }
         else
            {
            for (int32_t i = 1; i < node->getFirstChild()->getNumChildren(); i++) // node->getFirstChild()->getFirstChild() will remain anchored in the new clearNode
               {
               s->anchorNode(node->getFirstChild()->getChild(i), s->_curTree);
               }
            clearNode = TR::Node::create(TR::pdclear, 3,
                                        node->getFirstChild()->getFirstChild(),
                                        TR::Node::iconst(node, leftMostNibbleToClear),
                                        TR::Node::iconst(node, digitsToClear));
            clearNode->setDecimalPrecision(node->getDecimalPrecision());
            dumpOptDetails(s->comp(),"\tcreate new %s [0x%p] :  leftDigit %d, digitsToClear %d\n",
               clearNode->getOpCode().getName(),clearNode,leftMostNibbleToClear,digitsToClear);
            }
         if (clearNode)
            {
            return s->replaceNode(node, clearNode, s->_curTree);
            }
         }
      }
   return node;
   }

// TypeReduction can convert packed math to DFP math. If the packed math is used in cases where integer math might overflow,
// we could end up with something like pd2l -> dd2pd -> ddadd. Converting DFP to packed is expensive, so we should go
// directly from DFP to binary. Sometimes, due to commoning, we may also end up with a tree like
// pd2i -> zd2pd -> dd2zdSetSign (with this node commoned) -> ddX. In this case, it's still faster to break commoning and
// go from DFP to integer directly, setting the sign as needed.
static TR::Node *cancelDFPtoBCDtoBinaryConversion(TR::Node *node, TR::Block *block, TR::Simplifier *s)
   {
   TR::ILOpCodes op = node->getOpCodeValue();
   TR_ASSERT(op == TR::pd2l || op == TR::pd2lu || op == TR::pd2i || op == TR::pd2iu, "Node %s [0x%x] isn't a packed to binary conversion in cancelDFPtoBCDtoBinaryConversion\n",
           node->getOpCode().getName(), node);

   TR::Node *firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isConversion() && firstChild->getType().isBCD())
      {
      // See if there's any truncation
      int32_t targetPrec = std::min<int32_t>(node->getDataType().TR::DataType::getMaxPrecisionFromType() - 1, firstChild->getDecimalPrecision());
      int32_t sourcePrec = TR::getMaxSigned<TR::Int32>();
      TR::Node *setSignNode = NULL;
      bool truncAndSetSign = false;

      if (firstChild->getOpCode().isSetSign() || firstChild->getOpCode().isSetSignOnNode())
         setSignNode = firstChild;

      if (firstChild->hasSourcePrecision())
         {
         targetPrec = std::min(targetPrec, firstChild->getSourcePrecision());
         sourcePrec = firstChild->getSourcePrecision();
         }

      if (firstChild->getFirstChild()->getOpCode().isConversion() && firstChild->getFirstChild()->getType().isBCD())
         {
         firstChild = firstChild->getFirstChild();
         targetPrec = std::min<int32_t>(targetPrec, firstChild->getDecimalPrecision());
         sourcePrec = firstChild->getDecimalPrecision();
         if (firstChild->hasSourcePrecision())
            {
            targetPrec = std::min(targetPrec, firstChild->getSourcePrecision());
            sourcePrec = firstChild->getSourcePrecision();
            }

         if (setSignNode == NULL && (firstChild->getOpCode().isSetSign() || firstChild->getOpCode().isSetSignOnNode()))
            setSignNode = firstChild;
         }

      TR::Node *child = firstChild->getFirstChild();

      TR_RawBCDSignCode sign = raw_bcd_sign_unknown;
      if (setSignNode != NULL)
         {
         if (setSignNode->getOpCode().isSetSignOnNode())
            {
            sign = setSignNode->getSetSign();
            }
         else
            {
            TR::Node *signNode = setSignNode->getSetSignValueNode();
            if (!signNode->getOpCode().isIntegralConst())
               return node;
            int32_t rawSign = signNode->get32bitIntegralValue();
            if (rawSign == TR::DataType::getIgnoredSignCode())
               return node;

            sign = rawSignCodeMap[rawSign];
            }
         }

      if (!child->getType().isDFP() ||
          !performTransformation(s->comp(), "%sFold %s [0x%p] with child %s [0x%p]",
            s->optDetailString(),node->getOpCode().getName(),node,
            firstChild->getOpCode().getName(),firstChild))
         {
         return node;
         }

      // Only use a ddabs at ARCH(10) or below if we're not also truncating.
      // At ARCH(11), iabs is always faster; at ARCH(10) while truncating,
      // irem and ibs is faster
      bool useddabs = true;
      if (sourcePrec > targetPrec && setSignNode != NULL)
         useddabs = false;
      else if (s->comp()->cg()->supportsFastPackedDFPConversions())
         useddabs = false;

      TR::DataType intermediateType = TR::NoType;
      TR::Node *newConv = NULL;

      // Truncation is always fastest in binary. Ideally, we can convert to the final integral type and use a rem to truncate.
      // If necessary, we'll convert to a larger type, use rem, and then convert to a smaller type. intermediateType contains
      // the type used for the rem; if no rem is needed, it contains the final integral type.
      // Using a DFP modify precision is too slow, so bail out in that case.
      if (sourcePrec > targetPrec)
         {
         intermediateType = TR::DataType::getIntegralTypeFromPrecision(sourcePrec);
         if (intermediateType == TR::Int8 || intermediateType == TR::Int16)
            intermediateType = TR::Int32;

         if (intermediateType == TR::NoType)
            return node;
         }
      else
         {
         // If there's no truncation, the intermediate type and final type will be the same
         intermediateType = node->getDataType();
         }

      // We may have to generate none, one, or both of an abs/setNegative and a rem. A binary setNegative
      // is done with an ineg followed by an iabs. We always do the abs  first, in DFP on ARCH(10) for
      // performance, or using intermediateType on ARCH(11), because we need to generate a load positive
      // in the irem evaluator and we don't want to generate another for the iabs. Then we do the rem and
      // convert to the final type. Lastly, if we need an ineg, we generate that.
      if (setSignNode != NULL)
         {
         // Setting the sign to positive is fastest on ARCH(10) in DFP when we don't also have to truncate
         if (useddabs && (sign == raw_bcd_sign_0xf || sign == raw_bcd_sign_0xc))
            {
            TR::Node *newSSNode = TR::Node::create(node, TR::ILOpCode::absOpCode(child->getDataType()), 1);
            newSSNode->setAndIncChild(0, child);
            newSSNode->setIsNonNegative(true);
            child = newSSNode;
            }
         else if (sign == raw_bcd_sign_0xf || sign == raw_bcd_sign_0xc || sign == raw_bcd_sign_0xd)
            {
            // For all other cases, generate the setsign in binary. Always do the iabs first. If we also need
            // an ineg, do it after the irem.
            if (intermediateType != child->getDataType())
               {
               newConv = TR::Node::create(node, TR::ILOpCode::getProperConversion(child->getDataType(), intermediateType, node->getOpCode().isUnsigned()), 1);
               newConv->setAndIncChild(0, child);
               child = newConv;
               }

            TR::Node *newSSNode = TR::Node::create(node, TR::ILOpCode::absOpCode(intermediateType), 1);
            newSSNode->setAndIncChild(0, child);
            newSSNode->setIsNonNegative(true);
            child = newSSNode;
            }
         else
            {
            TR_ASSERT(false, "Invalid sign on node %s (0x%x)\n", firstChild->getOpCode().getName(), firstChild);
            return node;
            }
         }

      // Truncate using an irem or lrem
      if (sourcePrec > targetPrec)
         {
         TR::Node *dd2l = child;
         if (intermediateType != child->getDataType())
            {
            dd2l = TR::Node::create(node, TR::ILOpCode::getProperConversion(child->getDataType(), intermediateType, node->getOpCode().isUnsigned()), 1);
            dd2l->setAndIncChild(0, child);
            if (child->isNonNegative())
               dd2l->setIsNonNegative(true);
            }

         TR::Node *lrem = TR::Node::create(node, TR::ILOpCode::remainderOpCode(intermediateType), 2);
         lrem->setAndIncChild(0, dd2l);
         switch (intermediateType)
            {
            case TR::Int64:
               lrem->setAndIncChild(1, TR::Node::lconst( node, (int64_t)computePositivePowerOfTen(targetPrec)));
               break;
            case TR::Int32:
               lrem->setAndIncChild(1, TR::Node::iconst(node, (int32_t)computePositivePowerOfTen(targetPrec)));
               break;
            }

         child = lrem;
         }

      // Convert to the final type if needed
      if (child->getDataType() != node->getDataType())
         {
         newConv = TR::Node::create(node, TR::ILOpCode::getProperConversion(child->getDataType(), node->getDataType(), node->getOpCode().isUnsigned()), 1);
         newConv->setAndIncChild(0, child);
         child = newConv;
         }

      if (setSignNode != NULL && sign == raw_bcd_sign_0xd)
         {
         TR::Node *inegNode = TR::Node::create(node, TR::ILOpCode::negateOpCode(child->getDataType()), 1);
         inegNode->setAndIncChild(0, child);
         inegNode->setIsNonPositive(true);
         child = inegNode;
         }

      s->replaceNode(node, child, s->_curTree, true);
      dumpOptDetails(s->comp(), "\tinto %s [0x%x]\n", child->getOpCode().getName(), child);
      return child;
      }

   return node;
   }

// If a grandparent (node) doesn't require a cleaned sign and its child (that would pass through a sign -- like a shift or modPrec)
// is not used in any other context (firstChild) then this cleaning grandChild operation can be removed.
static TR::Node *removeGrandChildClean(TR::Node *node, TR::Block *block, TR::Simplifier *s)
   {
   // pd2i doesn't need a cleaned sign so remove any grandchild that cleans if child is not used in another context
   TR::Node *grandChild = NULL;
   TR::Node *firstChild = node->getFirstChild();
   if (firstChild->getReferenceCount() == 1 &&
       (firstChild->getOpCode().isPackedShift() || firstChild->getOpCode().isPackedModifyPrecision()) && // TODO: extend to other packed ops that pass-through the sign
       firstChild->getFirstChild()->getOpCodeValue() == TR::pdclean &&
       performTransformation(s->comp(), "%sRemove unneeded pdclean [" POINTER_PRINTF_FORMAT "] under parent %s [" POINTER_PRINTF_FORMAT "] and child %s [" POINTER_PRINTF_FORMAT "]\n",
         s->optDetailString(), firstChild->getFirstChild(),node->getOpCode().getName(),node,firstChild->getOpCode().getName(),firstChild))
      {
      grandChild = firstChild->getFirstChild();
      grandChild = firstChild->setChild(0,s->replaceNodeWithChild(grandChild, grandChild->getFirstChild(), s->_curTree, block));
      }
   return grandChild;
   }

static TR::Node *cancelPackedToIntegralConversion(TR::Node * node, TR::ILOpCodes reverseOp, TR::Block * block, TR::Simplifier * s)
   {
   bool isLong = node->getType().isInt64();
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *newNode = NULL;
   if (firstChild->getOpCodeValue() == reverseOp)
      {
      int32_t precision = firstChild->getDecimalPrecision();
      const int32_t maxPrecision = isLong ? TR::getMaxSignedPrecision<TR::Int64>() : TR::getMaxSignedPrecision<TR::Int32>();
      newNode = s->unaryCancelOutWithChild(node, firstChild, s->_curTree, reverseOp);
      if (newNode &&
          precision < maxPrecision)
         {
         // In addition to the (redundant) action of the pd2x/x2pd conversion if the x2pd precision is < 10/19 then there is
         // also a truncation side-effect
         // This side-effect must be maintained by introducing a remainder operation to correct to the final # of result digits
         uint64_t powOfTen = computePositivePowerOfTen(precision);
         newNode = TR::Node::create(isLong ? TR::lrem : TR::irem, 2,
                                   newNode,
                                   isLong ? TR::Node::lconst( node, (int64_t)powOfTen) :
                                            TR::Node::iconst(node, (int32_t)powOfTen));
         newNode->getFirstChild()->decReferenceCount(); // inc'd an extra time above
         newNode->incReferenceCount();
         }
      }
   return newNode;
   }

static void trackSetSignValue(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   if (!(node->getOpCode().isSetSign() || node->getOpCode().isSetSignOnNode()) || node->hasKnownSignCode())
      return;

   if (!node->getType().isEmbeddedSign()) // only 0xA -> 0xF type sign codes are currently tracked
      return;

   if (node->getOpCode().isSetSign())
      {
      TR::Node *setSignValueNode = node->getSetSignValueNode();
      if (setSignValueNode->getOpCode().isLoadConst())
         {
         int32_t sign = setSignValueNode->get32bitIntegralValue();
         if (sign >= TR::DataType::getFirstValidSignCode() && sign <= TR::DataType::getLastValidSignCode() &&
             performTransformation(s->comp(), "%sSet known sign value 0x%x on %s [" POINTER_PRINTF_FORMAT "]\n",s->optDetailString(),sign,node->getOpCode().getName(),node))
            {
            node->resetSignState();
            if (sign == 0xc)
               node->setKnownSignCode(raw_bcd_sign_0xc);
            else if (sign == 0xd)
               node->setKnownSignCode(raw_bcd_sign_0xd);
            else if (sign == 0xf)
               node->setKnownSignCode(raw_bcd_sign_0xf);
            }
         }
      }
   else if (node->getOpCode().isSetSignOnNode())
      {
      TR_RawBCDSignCode rawSign = node->getSetSign();
      if (performTransformation(s->comp(), "%sSet known sign value 0x%x on setSignOnNode %s [" POINTER_PRINTF_FORMAT "]\n",
            s->optDetailString(),TR::DataType::getValue(rawSign),node->getOpCode().getName(),node))
         {
         node->resetSignState();
         node->setKnownSignCode(rawSign);
         }
      }
   }

// Reduce the precision of a packed decimal arithmetic node based on the precision of its children, by inserting
// a pdModifyPrecision node which could perhaps be optimized away.
// eg. pdAdd (p=14) with children pdLoad (p=3) and pdLoad (p=4) will have p at most 5
static TR::Node *reducePackedArithmeticPrecision(TR::Node *node, int32_t maxComputedResultPrecision, TR::Simplifier *s)
   {
   int32_t nodePrecision = node->getDecimalPrecision();

   // attempt to reduce the node's precision to the max possible precision (based on operand precision)
   if (maxComputedResultPrecision < nodePrecision &&
       performTransformation(s->comp(), "%sReduce %s [" POINTER_PRINTF_FORMAT "] precision from %d to the maxComputedResultPrecision %d\n",
         s->optDetailString(), node->getOpCode().getName(), node, nodePrecision, maxComputedResultPrecision))
      {
      // it is not legal to reduce the precision directly as the parent (if it is a call for example) may require the larger node precision
      // the parent can optimize away the new pdModifyPrecision if allowed
      TR::Node *newArith = NULL;
      TR::ILOpCodes arithOp = node->getOpCodeValue();
      TR::Node::recreate(node, TR::pdModifyPrecision);
      node->setAndIncChild(0, newArith = TR::Node::create(arithOp, 2, node->getFirstChild(), node->getSecondChild()));
      node->setNumChildren(1);

      newArith->setDecimalPrecision(maxComputedResultPrecision);
      newArith->getFirstChild()->decReferenceCount();
      newArith->getSecondChild()->decReferenceCount();
      newArith->setFlags(node->getFlags());
      node->setFlags(0);
      dumpOptDetails(s->comp(), "%screated new %s [" POINTER_PRINTF_FORMAT "] with maxComputedResultPrecision %d and modify old %s [" POINTER_PRINTF_FORMAT "] to %s\n",
         s->optDetailString(), newArith->getOpCode().getName(), newArith, maxComputedResultPrecision, newArith->getOpCode().getName(), node, node->getOpCode().getName());
      return node;
      }

   return NULL;
   }

// Handles constant and non-constant compares
static TR_PackedCompareCondition packedCompareCommon(TR::Node *parent, TR::Node *op1, TR::Node *op2, bool ignoreConstSign, TR::Simplifier * s)
   {
   if (s->trace())
      traceMsg(s->comp(),"packedCompare: (parent  %s (%p) castedToBCD=%s, op1 %s (%p), op2 %s (%p), absVal=%s) %s vs %s",
         parent->getOpCode().getName(),parent,parent->castedToBCD()?"yes":"no",
         op1->getOpCode().getName(),op1,
         op2->getOpCode().getName(),op2,
         ignoreConstSign ? "true":"false",
         op1->getOpCode().getName(),
         op2->getOpCode().getName());

   // check some properties first before the general compare loop for constants
   bool op1IsZero = op1->isZero();
   bool op2IsZero = op2->isZero();
   if (op1IsZero && op2IsZero)
      {
      if (s->trace())
         traceMsg(s->comp(), " -> TR_PACKED_COMPARE_EQUAL (op1 == op2 == zero)\n");
      return TR_PACKED_COMPARE_EQUAL;
      }

   bool op1_GE_zero = op1->isNonNegative();
   bool op2_GE_zero = op2->isNonNegative();

   bool op1IsNegative = op1->isNonZero() && op1->isNonPositive();
   bool op2IsNegative = op2->isNonZero() && op2->isNonPositive();

   bool op1IsPositive = op1->isNonZero() && op1->isNonNegative();
   bool op2IsPositive = op2->isNonZero() && op2->isNonNegative();

   if (!ignoreConstSign)
      {
      if (op1IsNegative && op2_GE_zero)
         {
         if (s->trace())
            traceMsg(s->comp(), " -> TR_PACKED_COMPARE_LESS (op1 < 0 and op2 >= 0)\n");
         return TR_PACKED_COMPARE_LESS;
         }

      if (op1_GE_zero && op2IsNegative)
         {
         if (s->trace())
            traceMsg(s->comp(), " -> TR_PACKED_COMPARE_GREATER (op1 >= 0 and op2 < 0)\n");
         return TR_PACKED_COMPARE_GREATER;
         }
      }

   return TR_PACKED_COMPARE_UNKNOWN;
   }

static TR_PackedCompareCondition packedCompare(TR::Node *parent, TR::Node *op1, TR::Node *op2, TR::Simplifier * s)
   {
   return packedCompareCommon(parent, op1, op2, false, s); // ignoreConstSign=false;
   }

static TR::Node *simplifyPackedArithmeticOperand(TR::Node *node, TR::Node *parent, TR::Block * block, TR::Simplifier * s)
   {
   node = removeOperandWidening(node, parent, block, s);

   if (node->getType().isAnyPacked() &&
       node->canRemoveArithmeticOperand())
      {
      bool parentAllowsRemoval = true;
      // operation (e.g. an pdcmpxx) may have to be done as a logical operation so do not remove sign changing children as the comparison may then fail
      if (parent->castedToBCD())
         {
         if (s->trace())
            traceMsg(s->comp(),"parent %s (%p) castedToBCD=true for child %s (%p) so do not allow removal of child\n",
               parent->getOpCode().getName(),parent,node->getOpCode().getName(),node);
         parentAllowsRemoval = false;
         }

      if (parentAllowsRemoval &&
          performTransformation(s->comp(), "%sRemove unnecessary arithmetic operand %s [" POINTER_PRINTF_FORMAT "]\n",
                                  s->optDetailString(), node->getOpCode().getName(), node))
         {
         node = s->replaceNodeWithChild(node, node->getFirstChild(), s->_curTree, block);
         }
      }

   return node;
   }

static TR::Node *pdaddCommonSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   TR::Node *firstChild  = node->setChild(0, simplifyPackedArithmeticOperand(node->getFirstChild(), node, block, s));
   TR::Node *secondChild = node->setChild(1, simplifyPackedArithmeticOperand(node->getSecondChild(), node, block, s));

   return node;
   }

static TR::Node *propagateTruncationToConversionChild(TR::Node *node, TR::Simplifier * s, TR::Block *block)
   {
   // Zoned and unicode are both 'full byte' types so performing even precision truncations is easier
   // than in packed form where half a byte must be cleared.
   TR_ASSERT(node->getOpCode().isShift() || node->getOpCode().isModifyPrecision(),"propagateTruncationToConversionChild only valid for shifts or modifyPrecision ops\n");

   bool isShift = node->getOpCode().isShift();

   TR::Node *child = node->getFirstChild();

   if (isShift && !node->getSecondChild()->getOpCode().isLoadConst())
      return child;

   int32_t shiftAmount = isShift ? node->getSecondChild()->get32bitIntegralValue() : 0;

   if (node->getOpCode().isRightShift())
      shiftAmount = -shiftAmount;
   if (child->getReferenceCount() == 1 &&
       child->getOpCode().isConversion() && (child->getFirstChild()->getType().isAnyZoned() || child->getFirstChild()->getType().isAnyUnicode()) &&
       node->getDecimalPrecision() < (child->getDecimalPrecision() + shiftAmount))
      {
      // Reduce the child conversion precision to the just the digits that will survive the truncating operation
      int32_t survivingDigits = node->survivingDigits();
      if (survivingDigits > 0 &&
          performTransformation(s->comp(), "%sReduce %s child [" POINTER_PRINTF_FORMAT "] precision to %d due to truncating %s parent [" POINTER_PRINTF_FORMAT "]\n",
            s->optDetailString(),child->getOpCode().getName(),child,survivingDigits,node->getOpCode().getName(),node))
         {
         child->setDecimalPrecision(survivingDigits);
         child->setVisitCount(0);
         return s->simplify(child, block); // try to propagate the shrunken precision down the tree
         }
      }
   return child;
   }

static TR::Node *foldSetSignIntoGrandChild(TR::Node *setSign, TR::Block *block, TR::Simplifier *s)
   {
   // set sign operations are typically cheaper then the corresponding general sign moving operation (eg. zdshrSetSign vs zdshr). Although any iconst setSign value
   // would be valid in newSetSign node created below (as the pdsetsign operation dominates it) choose one that is the zone value (0xf) as this is most
   // likely to end up being being an effective nop during code generation. In the case where the pdSetSign is itself to the zone value then this pdSetSign
   // node can be removed completely.
   if (!setSign->getOpCode().isSetSign())
      {
      TR_ASSERT(false,"expecting node %p to be a setSign operation\n",setSign);
      return setSign;
      }

   TR::Node *child = setSign->getFirstChild();
   if (setSign->getReferenceCount() == 1 &&
       child->getReferenceCount() == 1 &&
       (child->getOpCodeValue() == TR::zd2pd || child->getOpCodeValue() == TR::pd2zd) &&
       child->getFirstChild()->getReferenceCount() == 1 &&
       TR::ILOpCode::setSignVersionOfOpCode(child->getFirstChild()->getOpCodeValue()) != TR::BadILOp &&
       performTransformation(s->comp(), "%sFold %s [" POINTER_PRINTF_FORMAT "] below child %s [" POINTER_PRINTF_FORMAT "] and into grandchild %s [" POINTER_PRINTF_FORMAT "] and create new ",
         s->optDetailString(),setSign->getOpCode().getName(),setSign,child->getOpCode().getName(),child,child->getFirstChild()->getOpCode().getName(),child->getFirstChild()))
      {
      TR::ILOpCodes setSignOp = TR::ILOpCode::setSignVersionOfOpCode(child->getFirstChild()->getOpCodeValue());
      int32_t index = TR::ILOpCode::getSetSignValueIndex(setSignOp);
      TR::Node *newNode = NULL;
      TR::Node *src = child->getFirstChild();
      int32_t newSetSignValue = TR::DataType::getIgnoredSignCode();
      bool removeSetSignNode = false;
      if ((setSign->getOpCodeValue() == TR::pdSetSign) &&
          setSign->getSecondChild()->getOpCode().isLoadConst() &&
          setSign->getSecondChild()->get64bitIntegralValue() == TR::DataType::getZonedValue())
         {
         newSetSignValue = TR::DataType::getZonedValue();
         removeSetSignNode = true;
         }
      TR::Node *paddingAddress = NULL;
      TR::Node *newSetSign = TR::Node::iconst(src, newSetSignValue);
      if (index == 1)
         {
         // Transform:
         //    pdSetSign
         //       zd2pd
         //          zdsle2zd - src
         //             x
         //       sign
         // to:
         //       zd2pd
         //          zdsle2zdSetSign - newNode
         //             x
         //             sign - newSetSign
         if (paddingAddress)
            newNode = TR::Node::create(setSignOp, 3,
                                      src->getFirstChild(),    // x
                                      newSetSign,              // setSignValue
                                      paddingAddress);
         else
            newNode = TR::Node::create(setSignOp, 2,
                                      src->getFirstChild(),    // x
                                      newSetSign);             // setSignValue
         src->getFirstChild()->decReferenceCount();
         }
      else if (index == 2)
         {
         // Transform:
         //    pdSetSign
         //       zd2pd
         //          zdshr - src
         //             x
         //             shift
         //       sign
         // to:
         //       zd2pd
         //          zdshrSetSign - newNode
         //             x
         //             shift
         //             sign - newSetSign
         if (paddingAddress)
            newNode = TR::Node::create(setSignOp, 4,
                                      src->getFirstChild(),    // x
                                      src->getSecondChild(),   // shiftAmount
                                      newSetSign,              // setSignValue
                                      paddingAddress);
         else
            newNode = TR::Node::create(setSignOp, 3,
                                      src->getFirstChild(),    // x
                                      src->getSecondChild(),   // shiftAmount
                                      newSetSign);             // setSignValue
         src->getFirstChild()->decReferenceCount();
         src->getSecondChild()->decReferenceCount();
         }
      else if (index == 3)
         {
         // Transform:
         //    zdSetSign
         //       pd2zd
         //          pdshr - src
         //             x
         //             shift
         //             round
         //       sign
         // to:
         //       pd2zd
         //          pdshrSetSign - newNode
         //             x
         //             shift
         //             round
         //             sign - newSetSign
         if (paddingAddress)
            newNode = TR::Node::create(setSignOp, 5,
                                      src->getFirstChild(),    // x
                                      src->getSecondChild(),   // shiftAmount
                                      src->getThirdChild(),    // roundAmount
                                      newSetSign,              // setSignValue
                                      paddingAddress);
         else
            newNode = TR::Node::create(setSignOp, 4,
                                      src->getFirstChild(),    // x
                                      src->getSecondChild(),   // shiftAmount
                                      src->getThirdChild(),    // roundAmount
                                      newSetSign);             // setSignValue
         src->getFirstChild()->decReferenceCount();
         src->getSecondChild()->decReferenceCount();
         src->getThirdChild()->decReferenceCount();
         }
      else
         {
         TR_ASSERT(false,"setSign index of %d not supported in foldSetSignIntoGrandChild\n",index);
         }
      if (paddingAddress)
         paddingAddress->decReferenceCount();
      if (newNode)
         {
         dumpOptDetails(s->comp(), "%s [" POINTER_PRINTF_FORMAT "] with paddingAddress [" POINTER_PRINTF_FORMAT "]", newNode->getOpCode().getName(),newNode,paddingAddress);
         if (removeSetSignNode)
            dumpOptDetails(s->comp(), " and remove parent %s node [" POINTER_PRINTF_FORMAT "]\n", setSign->getOpCode().getName(),setSign);
         else
            dumpOptDetails(s->comp(),"\n");
         newNode->incReferenceCount();
         newNode->setDecimalPrecision(src->getDecimalPrecision());
         stopUsingSingleNode(src, false, s); // removePadding=false, remains attached to newNode if one exists on src
         child->setChild(0, s->simplify(newNode, block));
         child->setVisitCount(0); // revisit this node
         if (removeSetSignNode)
            {
            setSign->getSecondChild()->recursivelyDecReferenceCount();
            stopUsingSingleNode(setSign, true, s); // removePadding=true
            child->setDecimalPrecision(setSign->getDecimalPrecision());
            return child;
            }
         else
            {
            return setSign;
            }
         }
      }
   return setSign;
   }

static bool propagateSignStateLeftShift(TR::Node *node, int32_t shiftAmount, TR::Block *block, TR::Simplifier *s)
   {
   if (!node->getType().isBCD())
      return false;

   if (node->getOpCode().isSetSign())
      return false;

   // propagate through all modify precisions
   bool validateModPrecOp = node->getOpCode().isModifyPrecision();

   // and propagate through all non-packed left shifts
   bool validateShiftOp = !validateModPrecOp &&
                          node->getOpCode().isLeftShift() &&
                          s->comp()->cg()->propagateSignThroughBCDLeftShift(node->getType());

   bool validateOp = validateModPrecOp || validateShiftOp;
   if (!validateOp)
      return false;

   TR::Node *child = node->getFirstChild();
   return propagateSignState(node, child, shiftAmount, block, s);
   }

//---------------------------------------------------------------------
// Packed decimal to integer
//

TR::Node *pd2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->setChild(0, simplifyPackedArithmeticOperand(node->getFirstChild(), node, block, s));

   TR::Node *newNode = cancelPackedToIntegralConversion(node, TR::i2pd, block, s);
   if (newNode)
      return newNode;

   newNode = cancelDFPtoBCDtoBinaryConversion(node, block, s);
   if (newNode != node)
      return newNode;

   firstChild = node->getFirstChild();
   if (firstChild->getOpCodeValue() == TR::pdclean)
      firstChild = node->setChild(0, s->replaceNodeWithChild(firstChild, firstChild->getFirstChild(), s->_curTree, block));

   removeGrandChildClean(node, block, s);

   firstChild = node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));

   if (!node->isNonNegative() &&
       node->getFirstChild()->isNonNegative() &&
       performTransformation(s->comp(), "%sSet x >= 0 flag on %s [" POINTER_PRINTF_FORMAT "] with x >= 0 child\n",
             s->optDetailString(),node->getOpCode().getName(),node))
      {
      node->setIsNonNegative(true);
      }

   return node;
   }

//---------------------------------------------------------------------
// Packed decimal to long integer (signed and unsigned)
//

TR::Node *pd2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   TR_ASSERT(node->getOpCodeValue() == TR::pd2l || node->getOpCodeValue() == TR::pd2lu,"expecting nodeOp to be pd2l or pd2lu\n");

   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->setChild(0, simplifyPackedArithmeticOperand(node->getFirstChild(), node, block, s));

   // lu2pd and l2pd are the exact same operation (and lu2pd is not even currently generated) so use l2pd as the reversed op for both pd2l/pd2lu
   // pd2l and pd2lu are only kept as separate ops as different runtime routines need to be called on C for these operations
   TR::ILOpCodes reverseOp = TR::l2pd;
   TR::Node *newNode = cancelPackedToIntegralConversion(node, reverseOp, block, s);
   if (newNode)
      return newNode;

   newNode = cancelDFPtoBCDtoBinaryConversion(node, block, s);
   if (newNode != node)
      return newNode;

   firstChild = node->getFirstChild();
   if (firstChild->getOpCodeValue() == TR::pdclean)
      firstChild = node->setChild(0, s->replaceNodeWithChild(firstChild, firstChild->getFirstChild(), s->_curTree, block));

   removeGrandChildClean(node, block, s);

   firstChild = node->getFirstChild();

   node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));

   return node;
   }

//---------------------------------------------------------------------
// Integer/Long integer to packed decimal (signed and unsigned)
//

TR::Node *i2pdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   TR::DataType sourceDataType = TR::NoType;
   TR::DataType targetDataType = TR::NoType;
   if (decodeConversionOpcode(node->getOpCode(), node->getDataType(), sourceDataType, targetDataType))
      {
      TR::ILOpCodes inverseOp = TR::ILOpCode::getProperConversion(targetDataType, sourceDataType, false /* !wantZeroExtension */); // inverse conversion to what we are given

      TR::Node * result;
      if ((result = s->unaryCancelOutWithChild(node, firstChild, s->_curTree, inverseOp)))
         return result;
      }

   if (!node->isNonNegative())
      {
      if (node->getFirstChild()->isNonNegative() &&
          performTransformation(s->comp(), "%sPropagate x >= 0 flag from %s [" POINTER_PRINTF_FORMAT "] to %s [" POINTER_PRINTF_FORMAT "]\n",
            s->optDetailString(),node->getFirstChild()->getOpCode().getName(),node->getFirstChild(),node->getOpCode().getName(),node))
         {
         node->setIsNonNegative(true);
         }
      else if ((node->getFirstChild()->getOpCodeValue() == TR::bu2l ||
                node->getFirstChild()->getOpCodeValue() == TR::su2l ||
                node->getFirstChild()->getOpCodeValue() == TR::iu2l ||
                node->getFirstChild()->getOpCodeValue() == TR::bu2i ||
                node->getFirstChild()->getOpCodeValue() == TR::su2i) &&
               performTransformation(s->comp(), "%sSet x >= 0 flag due on %s [" POINTER_PRINTF_FORMAT "] due to child %s [" POINTER_PRINTF_FORMAT "]\n",
                  s->optDetailString(),node->getOpCode().getName(),node,node->getFirstChild()->getOpCode().getName(),node->getFirstChild()))
         {
         node->setIsNonNegative(true);
         }
      }

   return node;
   }

TR::Node *pdaddSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   node = pdaddCommonSimplifier(node, block, s);

   if (node->getDecimalAdjust() != 0)
      return node;

   if (!node->isNonNegative() &&
       node->getFirstChild()->isNonNegative() && node->getSecondChild()->isNonNegative() &&
       performTransformation(s->comp(), "%sSet x >= 0 flag on %s [" POINTER_PRINTF_FORMAT "] with x >= 0 children\n",
         s->optDetailString(),node->getOpCode().getName(),node))
      {
      node->setIsNonNegative(true);
      return node; // try to clean up some upstream setsigns before proceeding with binary reduction
      }

   int32_t maxComputedResultPrecision = std::max(node->getFirstChild()->getDecimalPrecision(), node->getSecondChild()->getDecimalPrecision()) + 1;
   TR::Node *newNode = reducePackedArithmeticPrecision(node, maxComputedResultPrecision, s);
   if (newNode != NULL)
      return newNode;

   return node;
   }

TR::Node *pdsubSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node *firstChild  = node->setChild(0, simplifyPackedArithmeticOperand(node->getFirstChild(), node, block, s));
   TR::Node *secondChild = node->setChild(1, simplifyPackedArithmeticOperand(node->getSecondChild(), node, block, s));

   if (node->getDecimalAdjust() != 0)
      return node;

   firstChild = node->getFirstChild();
   secondChild = node->getSecondChild();
   if (secondChild->isZero() &&
       performTransformation(s->comp(),"%sReplace %s (0x%p) of isZero op2 %s (0x%p) with op1 %s (0x%p)\n",
         s->optDetailString(), node->getOpCode().getName(),node,secondChild->getOpCode().getName(),secondChild,firstChild->getOpCode().getName(),firstChild))
      {
      return s->replaceNodeWithChild(node, firstChild, s->_curTree, block);
      }

   if (firstChild->isZero() &&
       performTransformation(s->comp(), "%sStrength reduce %s [" POINTER_PRINTF_FORMAT "]  0 - %s [" POINTER_PRINTF_FORMAT "] to pdneg\n",
         s->optDetailString(),node->getOpCode().getName(),node,secondChild->getOpCode().getName(), secondChild))
      {
      node = TR::Node::recreate(node, TR::pdneg);
      node->setFlags(0);
      node->setChild(0,secondChild);
      node->setNumChildren(1);

      s->anchorNode(firstChild, s->_curTree);
      firstChild->recursivelyDecReferenceCount();
      return s->simplify(node, block);
      }

   int32_t maxComputedResultPrecision = std::max(node->getFirstChild()->getDecimalPrecision(), node->getSecondChild()->getDecimalPrecision()) + 1;
   TR::Node *newNode = reducePackedArithmeticPrecision(node, maxComputedResultPrecision, s);
   if (newNode != NULL)
      return newNode;

   return node;
   }

TR::Node *pdmulSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node *firstChild  = node->setChild(0, simplifyPackedArithmeticOperand(node->getFirstChild(), node, block, s));
   TR::Node *secondChild = node->setChild(1, simplifyPackedArithmeticOperand(node->getSecondChild(), node, block, s));

   if (node->getDecimalAdjust() != 0)
      return node;

   if (node->getSecondChild()->getSize() > node->getFirstChild()->getSize())
      {
      TR::Node *firstChild = node->getFirstChild();
      TR::Node *secondChild = node->getSecondChild();
      // helps make inlining of pdmul more likely if larger operand is first
      swapChildren(node, firstChild, secondChild, s);
      }
   else if (node->getSecondChild()->getSize() == node->getFirstChild()->getSize() &&
            node->getFirstChild()->getOpCode().isLoad() &&
            !node->getSecondChild()->getOpCode().isLoad())
      {
      // Try to get the more complex child as the first child so the result can naturally accumulate to the final receiver
      TR::Node *firstChild = node->getFirstChild();
      TR::Node *secondChild = node->getSecondChild();
      swapChildren(node, firstChild, secondChild, s);
      }

   int32_t maxComputedResultPrecision = node->getFirstChild()->getDecimalPrecision() + node->getSecondChild()->getDecimalPrecision();
   TR::Node *newNode = reducePackedArithmeticPrecision(node, maxComputedResultPrecision, s);
   if (newNode != NULL)
      return newNode;

   if (!node->isNonNegative() &&
       node->getFirstChild()->isNonNegative() && node->getSecondChild()->isNonNegative() &&
       performTransformation(s->comp(), "%sSet x >= 0 flag on %s [" POINTER_PRINTF_FORMAT "] with x >= 0 children\n",
         s->optDetailString(),node->getOpCode().getName(),node))
      {
      node->setIsNonNegative(true);
      return node; // try to clean up some upstream setsigns before proceeding with binary reduction
      }

   return node;
   }

TR::Node *pddivSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node *firstChild  = node->setChild(0, simplifyPackedArithmeticOperand(node->getFirstChild(), node, block, s));
   TR::Node *secondChild = node->setChild(1, simplifyPackedArithmeticOperand(node->getSecondChild(), node, block, s));

   if (node->getDecimalAdjust() != 0)
      return node;

   if (firstChild->getOpCode().isLoadConst() &&
       secondChild->getOpCode().isLoadConst() &&
       secondChild->isNonZero())
      {
      if (!node->getOpCode().isPackedDivide())
         return s->simplify(node, block);
      }

   if (!node->isNonNegative() &&
       firstChild->isNonNegative() && secondChild->isNonNegative() &&
       performTransformation(s->comp(), "%sSet x >= 0 flag on %s [" POINTER_PRINTF_FORMAT "] with x >= 0 children\n",
         s->optDetailString(),node->getOpCode().getName(),node))
      {
      node->setIsNonNegative(true);
      return node; // try to clean up some upstream setsigns before proceeding with binary reduction
      }

   TR::Node *newNode = reducePackedArithmeticPrecision(node, firstChild->getDecimalPrecision(), s);
   if (newNode != NULL)
      return newNode;

   return node;
   }

TR::Node *pdshrSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node *child = node->getFirstChild();

   if (node->getOpCodeValue() == TR::pdshr)
      {
      TR::Node *newNode = lowerPackedShiftOrSetSignBelowDFPConv(node, block, s);
      if (newNode != node)
         return newNode;
      }

   if (child->getOpCodeValue() == TR::pdSetSign)
      {
      TR::Node *newNode = foldSetSignIntoNode(child, true /* setSignIsTheChild */, node, true /* removeSetSign */, block, s);
      if (newNode != node)
         return newNode;
      }

   child = node->setChild(0, propagateTruncationToConversionChild(node,s,block));

   reduceShiftRightOverShiftRight(node, block, s);

   TR::ILOpCodes originalOp = node->getOpCodeValue();
   node = reduceShiftRightOverShiftLeft(node, block, s);
   if (node->getOpCodeValue() != originalOp)
      return s->simplify(node, block);

   TR::Node *newNode = createSetSignForKnownSignChild(node, block, s);
   if (newNode != node)
      return newNode;

   if (!node->isNonNegative() &&
       node->getOpCodeValue() == TR::pdshr &&
       node->getFirstChild()->isNonNegative() &&
       performTransformation(s->comp(), "%sSet x >= 0 flag on %s [" POINTER_PRINTF_FORMAT "] with x >= 0 children\n",
         s->optDetailString(),node->getOpCode().getName(),node))
      {
      node->setIsNonNegative(true);
      }

   child = node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));
   return node;
   }

TR::Node *pdloadSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   return node;
   }

// Also handles zdSetSign
TR::Node *pdSetSignSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   TR_ASSERT(node->getOpCodeValue() == TR::pdSetSign,
      "pdSetSignSimplifier only valid for pdSetSign nodes\n");
   simplifyChildren(node, block, s);
   node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));
   TR::Node *child = node->getFirstChild();

   bool isSetSignConstant = node->getSecondChild()->getOpCode().isLoadConst();
   int32_t sign = isSetSignConstant ? node->getSecondChild()->get32bitIntegralValue() : 0;

   if (child->hasKnownOrAssumedSignCode())
      {
      TR_RawBCDSignCode childSign = child->getKnownOrAssumedSignCode();
      if (TR::DataType::getValue(childSign) == sign &&
          performTransformation(s->comp(), "%sA1Fold %s [" POINTER_PRINTF_FORMAT "] and child %s [" POINTER_PRINTF_FORMAT "] with %s sign that matches setSign (both are 0x%x)\n",
            s->optDetailString(),node->getOpCode().getName(),node,child->getOpCode().getName(),child,child->hasKnownSignCode()?"known":"assumed",sign))
         {
         return s->replaceNodeWithChild(node, child, s->_curTree, block);
         }
      }

   TR_ASSERT(!isSetSignConstant || (sign == TR::DataType::getIgnoredSignCode() || (sign >= TR::DataType::getFirstValidSignCode() && sign <= TR::DataType::getLastValidSignCode())),
      "sign 0x%x is not a valid or ignored sign code on %s (%p)\n",sign,node->getOpCode().getName(),node);

   if (node->getDecimalPrecision() == child->getDecimalPrecision() &&
       child->isSimpleTruncation() &&
       performTransformation(s->comp(), "%sRemove simple truncating %s [" POINTER_PRINTF_FORMAT "] under setsign node %s [" POINTER_PRINTF_FORMAT "]\n",
         s->optDetailString(), child->getOpCode().getName(), child, node->getOpCode().getName(),node))
      {
      child = node->setChild(0, s->replaceNodeWithChild(child, child->getFirstChild(), s->_curTree, block, false)); // correctBCDPrecision=false // correctBCDPrecision=false because nodePrec==childPrec
      }

   if (child->getOpCode().isNeg() &&
       performTransformation(s->comp(),"%sRemove dominated %s [" POINTER_PRINTF_FORMAT "] under %s [" POINTER_PRINTF_FORMAT "]\n",
         s->optDetailString(), child->getOpCode().getName(), child, node->getOpCode().getName(),node))
      {
      child = node->setChild(0, s->replaceNodeWithChild(child, child->getFirstChild(), s->_curTree, block));
      }

   if (node->getOpCodeValue() == child->getOpCodeValue() &&
       performTransformation(s->comp(), "%s%s [" POINTER_PRINTF_FORMAT "] dominates setsign child %s [" POINTER_PRINTF_FORMAT "] -- remove child [" POINTER_PRINTF_FORMAT "]\n",
         s->optDetailString(),node->getOpCode().getName(),node,child->getOpCode().getName(),child,child))
      {
      // Transform:
      // pdSetSignA
      //    pdSetSignB
      //       pdX
      //       iconst 0xf
      //    iconst 0xc
      // to:
      // pdSetSignA
      //    pdX
      //    iconst 0xc
      child = node->setChild(0, s->replaceNodeWithChild(child, child->getFirstChild(), s->_curTree, block));
      }

   if (child->getOpCodeValue() == TR::pdclean &&
       performTransformation(s->comp(), "%s%s [" POINTER_PRINTF_FORMAT "] dominates cleaning child %s [" POINTER_PRINTF_FORMAT "] -- remove child [" POINTER_PRINTF_FORMAT "]\n",
         s->optDetailString(),node->getOpCode().getName(),node,child->getOpCode().getName(),child,child))
      {
      // Transform:
      // pdSetSign
      //    pdclean
      //       pdX
      //    iconst 0xf
      // to:
      // pdSetSign
      //    pdX
      //    iconst 0xf
      child = node->setChild(0, s->replaceNodeWithChild(child, child->getFirstChild(), s->_curTree, block));
      }

   if (child->getReferenceCount() == 1 && child->getOpCode().isSetSign() &&
       performTransformation(s->comp(), "%sFold %s [" POINTER_PRINTF_FORMAT "] value [" POINTER_PRINTF_FORMAT "] into dominated %s child [" POINTER_PRINTF_FORMAT "]\n",
         s->optDetailString(),node->getOpCode().getName(),node,node->getSecondChild(),child->getOpCode().getName(),child))

      {
      // Transform:
      //     pdSetSign
      //        xSetSign
      //           x
      //           setSignY
      //        setSignZ
      // to:
      //     xSetSign
      //        x
      //        setSignZ
      int32_t index = TR::ILOpCode::getSetSignValueIndex(child->getOpCodeValue());
      child->setChild(index, s->replaceNode(child->getChild(index),node->getSecondChild(), s->_curTree));
      child->resetSignState();
      node = s->replaceNodeWithChild(node, child, s->_curTree, block);
      return s->simplify(node, block);
      }
   else if (child->getReferenceCount() == 1 &&
            child->getOpCode().isSetSignOnNode() &&
            isSetSignConstant &&
            TR::DataType::isSupportedRawSign(sign) &&
            performTransformation(s->comp(), "%sFold %s [" POINTER_PRINTF_FORMAT "] sign=0x%x into dominated setSignOnNode %s child [" POINTER_PRINTF_FORMAT "]\n",
               s->optDetailString(),node->getOpCode().getName(),node,sign,child->getOpCode().getName(),child))

      {
      // Transform:
      //     pdSetSign
      //        xSetSign setSign=y
      //           x
      //        setSign z
      // to:
      //     xSetSign setSign=z
      //        x
      child->setSetSign(TR::DataType::getSupportedRawSign(sign));
      child->resetSignState();
      node = s->replaceNodeWithChild(node, child, s->_curTree, block);
      return s->simplify(node, block);
      }
   else if (TR::ILOpCode::setSignVersionOfOpCode(child->getOpCodeValue()) != TR::BadILOp)
      {
      // Transform:
      //     pdSetSign
      //        pdshl
      //           src
      //           shift
      //        sign
      // to:
      //    pdshlSetSign
      //       src
      //       shift
      //       sign
      TR::Node *newNode = foldSetSignIntoNode(node, false /* !setSignIsTheChild */, child, true /* removeSetSign */, block, s);
      if (newNode != node)
         return newNode;
      }

   child = node->getFirstChild();

   TR::Node *newNode = foldSetSignIntoGrandChild(node, block, s);
   if (newNode != node)
      return newNode;

   TR::ILOpCodes originalOp = node->getOpCodeValue();

   if (node->getOpCodeValue() == TR::pdSetSign)
      {
      newNode = lowerPackedShiftOrSetSignBelowDFPConv(node, block, s);
      if (newNode != node)
         return newNode;
      }

   if (node->getOpCodeValue() != originalOp)
      return s->simplify(node, block);

   child = node->getFirstChild();

   trackSetSignValue(node, block, s);

   return node;
   }

TR::Node *pdcleanSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   TR::Node *child = node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));

   simplifyChildren(node, block, s);

   child = node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));

   if (child->getOpCodeValue() == TR::pdclean || child->hasKnownOrAssumedCleanSign())
      {
      return s->replaceNodeWithChild(node, child, s->_curTree, block);
      }

   TR::ILOpCodes childOp = child->getOpCodeValue();
   // attempt to clean the sign of a setsign operation at compile time, for example
   // change a pdSetSign 0xF child to pdSetSign 0xC
   if (child->getReferenceCount() == 1 &&
       child->getOpCode().isSetSign() &&
       child->getSetSignValueNode()->getOpCode().isLoadConst())
      {
      int32_t index = TR::ILOpCode::getSetSignValueIndex(child->getOpCodeValue());
      TR::Node *setSignValueNode = child->getChild(index);
      int32_t rawSign = setSignValueNode->get32bitIntegralValue();
      int32_t cleanedSign = TR::DataType::getPreferredPlusCode();
      // Can only do this for setSigns to positive values because -0 is not clean
      if (TR::DataType::rawSignIsPositive(child->getDataType(), rawSign) &&
          performTransformation(s->comp(), "%sCleaning constant sign on child %s [" POINTER_PRINTF_FORMAT "] : 0x%x->0x%x by ",
            s->optDetailString(), child->getOpCode().getName(), child, rawSign, cleanedSign))
         {
         if (setSignValueNode->getReferenceCount() == 1)
            {
            dumpOptDetails(s->comp(),"modifying signNode %s [0x%p] value\n",setSignValueNode->getOpCode().getName(),setSignValueNode);
            setSignValueNode->set64bitIntegralValue(cleanedSign);
            }
         else
            {
            TR::Node *newSetSignValueNode = TR::Node::iconst(child, cleanedSign);
            dumpOptDetails(s->comp(),"creating new signNode %s [0x%p]\n",newSetSignValueNode->getOpCode().getName(),newSetSignValueNode);
            child->setChild(index, s->replaceNode(setSignValueNode, newSetSignValueNode, s->_curTree));
            }
         child->resetSignState();
         child->setHasKnownCleanSign(true);
         return s->replaceNodeWithChild(node, child, s->_curTree, block);
         }
      }

   // Fold pdclean over de2pd into de2pdClean at odd precisions only on ARCH(11). For even precisions, if the leftmost nibble
   // is non-zero, we won't clear it out until after the CPDT or CPXT, so they won't have the chance to clean the sign.
   if (s->comp()->cg()->supportsFastPackedDFPConversions() &&
       !node->hasIntermediateTruncation() &&
       child->getOpCode().isConversion() &&
       child->getFirstChild()->getType().isDFP() &&
       child->getDecimalFraction() == 0 &&
       !child->useCallForFloatToFixedConversion() &&
       (node->isOddPrecision() || (!node->isTruncating() && !child->isTruncating())))
      {
      //    pdclean
      //       de2pd
      //          deX
      // to:
      // de2pdClean
      //    deX
      //
      TR::ILOpCodes convOp = TR::ILOpCode::dfpToPackedCleanOp(child->getFirstChild()->getDataType(), node->getDataType());
      if (convOp != TR::BadILOp &&
          performTransformation(s->comp(), "%sFold %s (0x%p) above %s (0x%p) to ",s->optDetailString(),
            node->getOpCode().getName(),node,child->getOpCode().getName(),child))
         {
         TR::Node::recreate(node, convOp);
         dumpOptDetails(s->comp(), "%s\n",node->getOpCode().getName());
         node->setChild(0, s->replaceNodeWithChild(child, child->getFirstChild(), s->_curTree, block, false));
         if (child->hasSourcePrecision())
            node->setSourcePrecision(child->getSourcePrecision());
         return node;
         }
      }

   // since only oddPrecision cases can be handled for supportsFastPackedDFPConversions with de2pdClean in the transformation above
   // attempt to swing the pdclean below to a ddclean/declean regardless of supportsFastPackedDFPConversions
   if ((child->getOpCodeValue() == TR::de2pd || child->getOpCodeValue() == TR::dd2pd) && // avoid doing for de2pdClean/dd2pdClean
       !child->getFirstChild()->isNonNegative() &&
       !child->isTruncating()) // otherwise will need a post clean anyway so no point inserting a declean/ddclean
      {
      bool isSubtractSpecialCase = false;
      if (child->getFirstChild()->getOpCode().isSub() &&
          child->getFirstChild()->getFirstChild()->isNonNegative() &&
          child->getFirstChild()->getSecondChild()->isNonNegative())
         {
         // Although the ddsub cannot be marked with isNonNegative itself there still will not be a negative zero for this case
         // when dd2pd is not truncated (already checked above)
         // dd2pd  (not truncating)
         //   ddsub
         //     ddX X>=0
         //     ddY X>=0

         isSubtractSpecialCase = true;
         }

      TR::ILOpCodes cleanOp = TR::ILOpCode::cleanOpCode(child->getFirstChild()->getDataType());
      if (cleanOp != TR::BadILOp &&
          !isSubtractSpecialCase &&
          performTransformation(s->comp(), "%sInsert dfpClean under %s (0x%p) and remove %s (0x%p)\n",s->optDetailString(),
            child->getOpCode().getName(),child,node->getOpCode().getName(),node))
         {
         // change:
         //
         // pdclean
         //    de2pd
         //       deX
         //
         // to:
         //
         // pdclean (and remove this node)
         //    de2pd
         //       declean
         //          deX
         //
         TR::Node *dfpClean = TR::Node::create(child, cleanOp, 1);
         dumpOptDetails(s->comp(),"%sCreate new %s (0x%p) and setHasKnownCleanSign on %s (0x%p)\n",
            s->optDetailString(),dfpClean->getOpCode().getName(),dfpClean,child->getOpCode().getName(),child);
         dfpClean->setChild(0, child->getFirstChild());
         child->setAndIncChild(0, dfpClean);
         child->setHasKnownCleanSign(true);
         return s->replaceNodeWithChild(node, child, s->_curTree, block); // pdclean no longer needed
         }
      }

   if (child->hasKnownOrAssumedSignCode())
      {
      TR_RawBCDSignCode sign = child->getKnownOrAssumedSignCode();
      TR_ASSERT(TR::DataType::getPreferredPlusCode() == 0x0c,"expecting the preferred plus sign code 0x%x to be 0xc (==raw_bcd_sign_0xc)\n",TR::DataType::getPreferredPlusCode());
      switch (sign)
         {
         // Can only do this for setSigns to positive values because -0 is not clean
         case raw_bcd_sign_0xc:
            {
            return s->replaceNodeWithChild(node, child, s->_curTree, block);
            break;
            }
         case raw_bcd_sign_0xf:
            {
            bool removeClean = true;
            bool isLoadVarChild = child->getOpCode().isLoadVar();
            TR::Node *parent = s->_curTree ? s->_curTree->getNode() : NULL;
            if (isLoadVarChild &&
                parent &&
                parent->getOpCode().isPackedStore() &&
                parent->getValueChild() == node)
               {
               // the pdstoreSimplifier will attempt to change
               //
               // pdstore
               //    pdclean
               //       pdload sign=0xf
               // to:
               // pdstore
               //       pdSetSign
               //          pdload sign=0xf
               //          iconst 0xc
               //
               // as this will usually result in better codegen (e.g. move and clean can be done by one instruction)
               // so prevent reversing this optimization here (otherwise both opts keep happening and flipping the IL back and forth)
               if (s->trace())
                  traceMsg(s->comp(),"do not replace %s (%p) with setsign 0xf because parent %s (%p) is a store and child %s (%p) is a loadVar\n",
                     node->getOpCode().getName(),node,
                     parent->getOpCode().getName(),parent,
                     child->getOpCode().getName(),child);
               removeClean = false;
               }

            if (removeClean &&
                performTransformation(s->comp(), "%sFold pdclean [" POINTER_PRINTF_FORMAT "] of child [" POINTER_PRINTF_FORMAT "] with known sign 0xf to new pdSetSign ",
                  s->optDetailString(),node,child))
               {
               TR::Node *setSignNode = TR::Node::create(TR::pdSetSign, 2, child, TR::Node::iconst(child, TR::DataType::getPreferredPlusCode()));
               dumpOptDetails(s->comp(), "[" POINTER_PRINTF_FORMAT "]\n", setSignNode);
               setSignNode->setDecimalPrecision(node->getDecimalPrecision());
               setSignNode->setKnownSignCode(raw_bcd_sign_0xc);
               return s->replaceNode(node, setSignNode, s->_curTree, false); // needAnchor=false as child is anchored under setSignNode
               }
            break;
            }
         default: break;
         }
      }

   child = node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));

   if (node->getOpCodeValue() == TR::pdclean &&
       child->getReferenceCount() == 1 &&
       child->isSimpleTruncation() &&
       child->getFirstChild()->getOpCodeValue() == TR::pdclean &&
       performTransformation(s->comp(),"%sRemove pdclean [" POINTER_PRINTF_FORMAT "] under simple truncating %s [" POINTER_PRINTF_FORMAT "] as pdclean [" POINTER_PRINTF_FORMAT "] will clean\n",
         s->optDetailString(),child->getFirstChild(),child->getOpCode().getName(),child,node))
      {
      TR::Node *pdclean = child->getFirstChild();
      child->setChild(0, s->replaceNodeWithChild(pdclean, pdclean->getFirstChild(), s->_curTree, block));
      }

   node->setHasKnownCleanSign(true);
   return node;
   }

TR::Node *pdclearSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node *firstChild = node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));

   if (firstChild->getOpCodeValue() == TR::pdSetSign &&
       firstChild->hasKnownOrAssumedSignCode() &&
       performTransformation(s->comp(), "%sFold child %s [" POINTER_PRINTF_FORMAT "] into parent %s [" POINTER_PRINTF_FORMAT "] with sign 0x%x\n",
         s->optDetailString(),firstChild->getOpCode().getName(),firstChild,node->getOpCode().getName(),node,TR::DataType::getValue(firstChild->getKnownOrAssumedSignCode())))
      {
      TR_RawBCDSignCode childSign = firstChild->getKnownOrAssumedSignCode();
      TR::Node::recreate(node, TR::pdclearSetSign);
      node->setFlags(0);
      node->resetSignState();
      node->setSetSign(childSign);
      firstChild = node->setChild(0, s->replaceNodeWithChild(firstChild, firstChild->getFirstChild(), s->_curTree, block));
      return s->simplify(node, block);
      }
   return node;
   }

TR::Node *pdclearSetSignSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node *firstChild = node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));
   trackSetSignValue(node, block, s);
   return node;
   }

/**
 * \note `getFutureUseCountq in Simplifier may not be correct if simplify is
 * called more than once on a node so only use this routine for performance and
 * not functional purposes
 */
static bool isFirstOrOnlyReference(TR::Node *node)
   {
   return node->getReferenceCount() == 1
      || (node->getReferenceCount()-1 == node->getFutureUseCount());
   }


// Also handles ipdstore, zdstore, izdstore
TR::Node *pdstoreSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node *valueChild = node->setValueChild(removeOperandWidening(node->getValueChild(), node, block, s));

   if (!s->comp()->getOption(TR_KeepBCDWidening) &&
       valueChild->getOpCodeValue() == TR::pdclean &&
       node->getDecimalPrecision() <= TR::DataType::getMaxPackedDecimalPrecision() &&
       isFirstOrOnlyReference(valueChild) &&
       performTransformation(s->comp(), "%sFold pdclean [" POINTER_PRINTF_FORMAT "] into pdstore by setting CleanSignInPDStoreEvaluator flag on pdstore [" POINTER_PRINTF_FORMAT "]\n",
          s->optDetailString(), valueChild, node))
      {
      // this transformation is only done for first/only references to avoid a sub-optimality where an already clean sign is cleaned again on a subsequent reference
      // pdx
      //    pdclean  // firstRef
      //...
      // pdstore
      //    =>pdclean // if this clean is folded into its parent store then the already cleaned pdclean will be cleaned again
      //
      node->setCleanSignInPDStoreEvaluator(true);
      valueChild = node->setValueChild(s->replaceNodeWithChild(valueChild, valueChild->getFirstChild(), s->_curTree, block));
      }
   else if (valueChild->getOpCodeValue() == TR::pdSetSign &&
            valueChild->getSecondChild()->getOpCode().isLoadConst() &&
            valueChild->getSecondChild()->get64bitIntegralValue() == TR::DataType::getPreferredPlusCode() &&
            valueChild->getFirstChild()->getOpCode().isLoadVar() &&
            valueChild->getFirstChild()->hasKnownOrAssumedSignCode() &&
            valueChild->getFirstChild()->getKnownOrAssumedSignCode() == raw_bcd_sign_0xf &&
            node->getDecimalPrecision() <= TR::DataType::getMaxPackedDecimalPrecision() &&
            valueChild->getDecimalPrecision() >= valueChild->getFirstChild()->getDecimalPrecision() &&
            performTransformation(s->comp(), "%sFold pdsetsign [" POINTER_PRINTF_FORMAT "] by 0xc of child %s [" POINTER_PRINTF_FORMAT "]",
               s->optDetailString(), valueChild, valueChild->getFirstChild()->getOpCode().getName(), valueChild->getFirstChild()))
         {
         // pdstore
         //    pdsetsign
         //       pdload sign=0xf
         //       iconst 0xc
         // into
         //
         // pdstore clean
         //    pdload sign=0xf
         //
         // or
         //
         // pdstore
         //    pdclean
         //
         // cheaper to do load/store/setSign0xc all as one op (i.e. with a ZAP)
         //
         dumpOptDetails(s->comp(), " by setting CleanSignInPDStoreEvaluator flag on pdstore [" POINTER_PRINTF_FORMAT "]\n",node);
         node->setCleanSignInPDStoreEvaluator(true);
         valueChild = node->setValueChild(s->replaceNodeWithChild(valueChild, valueChild->getFirstChild(), s->_curTree, block));
         }

   valueChild = node->setValueChild(removeOperandWidening(valueChild, node, block, s));

   // no need to clean the input of the valueChild if this store will itself clean
   if (node->getType().isAnyPacked() &&
       node->mustCleanSignInPDStoreEvaluator() &&
       valueChild->getReferenceCount() == 1 &&
       valueChild->isSimpleTruncation() &&
       valueChild->getFirstChild()->getOpCodeValue() == TR::pdclean &&
       performTransformation(s->comp(),"%sRemove pdclean [" POINTER_PRINTF_FORMAT "] under simple truncating %s [" POINTER_PRINTF_FORMAT "] as pdstore [" POINTER_PRINTF_FORMAT "] will clean\n",
         s->optDetailString(),valueChild->getFirstChild(),valueChild->getOpCode().getName(),valueChild,node))
      {
      TR::Node *pdclean = valueChild->getFirstChild();
      valueChild->setChild(0, s->replaceNodeWithChild(pdclean, pdclean->getFirstChild(), s->_curTree, block));
      }

   if (node->getType().isAnyPacked() && node->mustCleanSignInPDStoreEvaluator())
      removeGrandChildClean(node, block, s);

   bool removeSimpleTruncation = (node->getDecimalPrecision() == valueChild->getDecimalPrecision()) &&
                                 valueChild->isSimpleTruncation();
   if (node->getType().isAnyPacked() && removeSimpleTruncation &&
       (node->mustCleanSignInPDStoreEvaluator() || node->isEvenPrecision()))
      {
      removeSimpleTruncation = false;
      }
   if (removeSimpleTruncation &&
       performTransformation(s->comp(), "%sRemove simple truncating %s [" POINTER_PRINTF_FORMAT "] under store node %s [" POINTER_PRINTF_FORMAT "]\n",
         s->optDetailString(), valueChild->getOpCode().getName(), valueChild, node->getOpCode().getName(),node))
      {
      valueChild = node->setValueChild(s->replaceNodeWithChild(valueChild, valueChild->getFirstChild(), s->_curTree, block, false)); // correctBCDPrecision=false because nodePrec==valueChildPrec
      }

   valueChild = node->setValueChild(removeOperandWidening(valueChild, node, block, s));

   // avoid swapping on subsequent references as this swapping is only needed for the initial evaluation
   // if the store symRefs are as below then the swapping on the 2nd reference will actually swap the children
   // in the opposite way needed for best performance and the swaps will thrash every time in the simplifier
   // pdstore symRef_A
   //    pdadd          -> only swap for this reference if needed (not needed here)
   //       pdload symRef_A
   //       pdload symRef_B
   // pdstore symRef_B
   //    =>pdadd        -> swapping for this reference will flip the children to the sub-optimal configuration
   if (isFirstOrOnlyReference(valueChild))
      s->comp()->cg()->swapChildrenIfNeeded(node, (char *) s->optDetailString());

   return node;
   }

// Handles f2pd, d2pd, e2pd
TR::Node *f2pdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   if (node->getFirstChild()->getOpCode().isLoadConst())
      {
      TR::ILOpCodes originalOp = node->getOpCodeValue();
      if (node->getOpCodeValue() != originalOp)
         return s->simplify(node, block);
      }

   return node;
   }

// Handles pd2f, pd2d, pd2e
TR::Node *pd2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));

   if (node->getFirstChild()->getOpCode().isLoadConst())
      {
      TR::ILOpCodes originalOp = node->getOpCodeValue();
      if (node->getOpCodeValue() != originalOp)
         return s->simplify(node, block);
      }

   return node;
   }

TR::Node *pdshlSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));

   bool isShift = node->getOpCode().isShift();
   bool isModifyPrecision = node->getOpCode().isModifyPrecision();

   if (node->getOpCodeValue() == TR::pdshl)
      {
      TR::Node *newNode = lowerPackedShiftOrSetSignBelowDFPConv(node, block, s);
      if (newNode != node)
         return newNode;
      }

   TR::Node * secondChild = isShift ? node->getSecondChild() : NULL;

   int32_t shiftAmount = -1;
   bool isShiftAmountConstant = isShift ? secondChild->getOpCode().isLoadConst() : false;
   if (isShiftAmountConstant)
      shiftAmount = secondChild->get32bitIntegralValue();
   else if (isModifyPrecision)
      shiftAmount = 0;

   bool isShiftByZero = (shiftAmount == 0);

   if (isShiftByZero)
      {
      bool isTruncation = node->getDecimalPrecision() < firstChild->getDecimalPrecision();
      if (node->getDecimalPrecision() >= firstChild->getDecimalPrecision()  &&
         node->getSize() == firstChild->getSize())
         {
         // The top nibble of an even precision number is zero so remove a one digit 'even to odd' precision widening
         // pdshl prec=x+1,size=y
         //   firstChild prec=x,size=y
         //   iconst 0
         // to:
         // firstChild
         // Also remove completely redundant pdshl nodes (no shift amount and no change in precision)
         return s->replaceNodeWithChild(node, firstChild, s->_curTree, block, false); // correctBCDPrecision=false because this is the special case of same byte widening
         }
      else if (node->getDecimalPrecision() <= firstChild->getDecimalPrecision() &&
               node->getFirstChild()->isSimpleTruncation() &&
               performTransformation(s->comp(), "%sRemove simple truncating firstChild %s [" POINTER_PRINTF_FORMAT "] of simple truncating node %s [" POINTER_PRINTF_FORMAT "]\n",
                  s->optDetailString(),firstChild->getOpCode().getName(),firstChild,node->getOpCode().getName(),node))

         {
         firstChild = node->setChild(0,s->replaceNodeWithChild(node->getFirstChild(), node->getFirstChild()->getFirstChild(), s->_curTree, block, false)); // correctBCDPrecision=false this is a trunc of a trunc
         return s->simplify(node, block);
         }
      else if ((
                firstChild->getOpCodeValue() == TR::zd2pd    ||
                firstChild->getOpCodeValue() == TR::zdsle2pd ||
                firstChild->getOpCodeValue() == TR::zdsls2pd ||
                firstChild->getOpCodeValue() == TR::zdsts2pd ||
                firstChild->getOpCodeValue() == TR::ud2pd    ||
                firstChild->getOpCodeValue() == TR::udsl2pd  ||
                firstChild->getOpCodeValue() == TR::udst2pd  ||
                firstChild->getOpCodeValue() == TR::pdSetSign ||
                firstChild->getOpCodeValue() == TR::pdclear ||
                firstChild->getOpCodeValue() == TR::pdclearSetSign ||
                firstChild->getOpCodeValue() == TR::pdneg ||
                firstChild->getOpCodeValue() == TR::df2pd ||
                firstChild->getOpCodeValue() == TR::dd2pd ||
                firstChild->getOpCodeValue() == TR::de2pd) &&
               (firstChild->getReferenceCount() == 1) &&
               isTruncation &&
               performTransformation(s->comp(), "%sRemove simple truncating %s [" POINTER_PRINTF_FORMAT "] of %s child [" POINTER_PRINTF_FORMAT "] by 0 and set child precision to %d\n",
                  s->optDetailString(),node->getOpCode().getName(),node,firstChild->getOpCode().getName(),firstChild,node->getDecimalPrecision()))
         {
         firstChild->setDecimalPrecision(node->getDecimalPrecision());
         node = s->replaceNodeWithChild(node, firstChild, s->_curTree, block);
         return s->simplify(node, block);
         }
      }
   firstChild = node->getFirstChild();

   if (!isShiftByZero)
      {
      TR::ILOpCodes originalOp = node->getOpCodeValue();
      node = reduceShiftLeftOverShiftRight(node, block, s);
      if (node->getOpCodeValue() != originalOp)
         return s->simplify(node, block);
      }

   firstChild = node->getFirstChild();

   if (!node->isNonNegative() &&
       (node->getOpCodeValue() == TR::pdshl || node->getOpCodeValue() == TR::pdModifyPrecision) &&
       firstChild->isNonNegative() &&
       performTransformation(s->comp(), "%sSet x >= 0 flag on %s [" POINTER_PRINTF_FORMAT "] with x >= 0 children\n",
         s->optDetailString(),node->getOpCode().getName(),node))
      {
      node->setIsNonNegative(true);
      return node; // try to clean up some upstream setsigns before proceeding with binary reduction
      }

   // do not do this for simple widenings as these will likely be removed altogether
   if (!isShiftByZero && firstChild->getOpCodeValue() == TR::pdSetSign)
      {
      TR::Node *newNode = foldSetSignIntoNode(firstChild, true, node, true, block, s); // setSignIsTheChild=true, removeSetSign=true
      if (newNode != node)
         return newNode;
      }

   if (shiftAmount >= 0)
      {
      bool changedSignState = propagateSignStateLeftShift(node, shiftAmount, block, s);
      // return now if some sign state change was made to see if a parent pdclean/pdsetsign operation can be removed before possibly converting pdshl to zdshl below
      if (changedSignState)
         return node;
      }

   firstChild = node->setChild(0, propagateTruncationToConversionChild(node,s,block));

   TR::Node *newNode = createSetSignForKnownSignChild(node, block, s);
   if (newNode != node)
      return newNode;

   firstChild = node->setChild(0, propagateTruncationToConversionChild(node,s,block));

   newNode = createSetSignForKnownSignChild(node, block, s);
   if (newNode != node)
      return newNode;

   firstChild = node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));
   return node;
   }

TR::Node *pdshlSetSignSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));
   node->setChild(0, propagateTruncationToConversionChild(node,s,block));

   TR::Node *firstChild = node->getFirstChild();

   // pdshlSetSign
   //    pdSetSign
   //       pdX
   //       iconst 0xF
   //    iconst shift
   //    iconst 0xF
   //
   // remove the pdSetSign as it is dominated in this context
   //
   if (firstChild->getOpCodeValue() == TR::pdSetSign &&
       firstChild->getReferenceCount() == 1 &&  // not required for correctness but the uncommoning of the pdSetSign in rc>1 cases can lead to worse code -- it is sure win in the rc=1 case
       firstChild->getSecondChild()->getOpCode().isLoadConst() && // pdSetSign setSign value
       node->getThirdChild()->getOpCode().isLoadConst() &&        // pdshlSetSign setSign value
       firstChild->getSecondChild()->get64bitIntegralValue() == node->getThirdChild()->get64bitIntegralValue() &&
       !node->hasIntermediateTruncation() &&
       performTransformation(s->comp(),"%sRemove dominated setSign %s [" POINTER_PRINTF_FORMAT "] under %s [" POINTER_PRINTF_FORMAT "] (both signs are 0x%x)\n",
         s->optDetailString(),firstChild->getOpCode().getName(),firstChild,node->getOpCode().getName(),node,(int32_t)node->getThirdChild()->get64bitIntegralValue()))
      {
      firstChild = node->setChild(0, s->replaceNodeWithChild(firstChild, firstChild->getFirstChild(), s->_curTree, block));
      }

   if (node->getOpCodeValue() == TR::pdshlSetSign)
      {
      TR::Node *newNode = lowerPackedShiftOrSetSignBelowDFPConv(node, block, s);
      if (newNode != node)
         return newNode;
      }

   if (node->getSecondChild()->getOpCode().isLoadConst() &&
       node->getSecondChild()->get64bitIntegralValue() == 0 &&
       node->getThirdChild()->getOpCode().isLoadConst())
      {
      if (firstChild->hasKnownOrAssumedSignCode())
         {
         int32_t sign = node->getThirdChild()->get32bitIntegralValue();
         TR_RawBCDSignCode childSign = firstChild->getKnownOrAssumedSignCode();
         if (TR::DataType::getValue(childSign) == sign &&
             performTransformation(s->comp(), "%sFold %s [" POINTER_PRINTF_FORMAT "] and child %s [" POINTER_PRINTF_FORMAT "] with %s sign that matches setSign (both are 0x%x)\n",
               s->optDetailString(),node->getOpCode().getName(),node,firstChild->getOpCode().getName(),firstChild,firstChild->hasKnownSignCode()?"known":"assumed",sign))
            {
            return s->replaceNodeWithChild(node, firstChild, s->_curTree, block);
            }
         }
      }

   TR::ILOpCodes originalOp = node->getOpCodeValue();
   node = reduceShiftLeftOverShiftRight(node, block, s);
   if (node->getOpCodeValue() != originalOp)
      return s->simplify(node, block);

   firstChild = node->getFirstChild();

   node = foldAndReplaceDominatedSetSign(node, false /* setSignIsTheChild */, node->getFirstChild(), block, s);

   TR::Node *newNode = foldSetSignIntoGrandChild(node, block, s);
   if (newNode != node)
      return newNode;

   trackSetSignValue(node, block, s);

   return node;
   }

TR::Node *pdshrSetSignSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));
   node->setChild(0, propagateTruncationToConversionChild(node,s,block));

   TR::Node *firstChild = node->getFirstChild();

   if (node->getOpCodeValue() == TR::pdshrSetSign)
      {
      TR::Node *newNode = lowerPackedShiftOrSetSignBelowDFPConv(node, block, s);
      if (newNode != node)
         return newNode;
      }

   if (firstChild->getOpCodeValue() == TR::pdSetSign &&
       firstChild->hasKnownOrAssumedSignCode() &&
       node->getChild(3)->getOpCode().isLoadConst())
      {
      int32_t sign = node->getChild(3)->get32bitIntegralValue();
      TR_RawBCDSignCode childSign = firstChild->getKnownOrAssumedSignCode();
      if (TR::DataType::getValue(childSign) == sign &&
          performTransformation(s->comp(), "%sFold %s [" POINTER_PRINTF_FORMAT "] and child %s [" POINTER_PRINTF_FORMAT "] with %s sign that matches setSign (both are 0x%x)\n",
            s->optDetailString(),node->getOpCode().getName(),node,firstChild->getOpCode().getName(),firstChild,firstChild->hasKnownSignCode()?"known":"assumed",sign))
         {
         firstChild = node->setChild(0, s->replaceNodeWithChild(firstChild, firstChild->getFirstChild(), s->_curTree, block));
         }
      }

   reduceShiftRightOverShiftRight(node, block, s);

   TR::ILOpCodes originalOp = node->getOpCodeValue();
   node = reduceShiftRightOverShiftLeft(node, block, s);
   if (node->getOpCodeValue() != originalOp)
      return s->simplify(node, block);

   firstChild = node->getFirstChild();

   node = foldAndReplaceDominatedSetSign(node, false /* setSignIsTheChild */, node->getFirstChild(), block, s);

   TR::Node *newNode = foldSetSignIntoGrandChild(node, block, s);
   if (newNode != node)
      return newNode;

   trackSetSignValue(node, block, s);

   return node;
   }

// Handles pd2df, pd2dd, pd2de
TR::Node *pd2dfSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));

   bool isAbs = false;
   if (node->getOpCodeValue() == TR::pd2dfAbs || node->getOpCodeValue() == TR::pd2ddAbs || node->getOpCodeValue() == TR::pd2deAbs)
      isAbs = true;

   TR::Node *child = node->getFirstChild();

   // pd2dd over dd2pd: both can be cancelled
   if (!isAbs)
      {
      TR::Node *result = s->unaryCancelOutWithChild(node, child, s->_curTree, TR::ILOpCode::getProperConversion(node->getDataType(), child->getDataType(), false));
      if (result != NULL)
         return result;
      }

   // pd2de over dd2pd: can go from dd2de directly if dd2de doesn't truncate due to type change or dd2pd doesn't explicitly truncate
   bool isNonTruncatingConversionPair = false;
   if (node->getOpCodeValue() == TR::pd2de &&
       (child->getOpCodeValue() == TR::df2pd || child->getOpCodeValue() == TR::dd2pd))
      isNonTruncatingConversionPair = true;
   else if (node->getOpCodeValue() == TR::pd2dd &&
            child->getOpCodeValue() == TR::df2pd)
      isNonTruncatingConversionPair = true;
   else if (node->getOpCodeValue() == TR::pd2dd &&
            child->getOpCodeValue() == TR::de2pd &&
            child->getDecimalPrecision() <= TR::DataType::getMaxLongDFPPrecision())
      isNonTruncatingConversionPair = true;
   else if (node->getOpCodeValue() == TR::pd2df &&
            (child->getOpCodeValue() == TR::dd2pd || child->getOpCodeValue() == TR::de2pd) &&
            child->getDecimalPrecision() <= TR::DataType::getMaxShortDFPPrecision())
      isNonTruncatingConversionPair = true;

   if (isNonTruncatingConversionPair &&
       !child->isTruncating() &&
       child->getFirstChild()->getType().isDFP() &&
       !child->getOpCode().isSetSignOnNode() && // Exclude dd2pdSetSign
       node->getDataType() != child->getFirstChild()->getDataType() &&
       node->getDecimalFraction() == 0 &&
       performTransformation(s->comp(), "%sFold %s (0x%p) and child %s (0x%p) to ",s->optDetailString(),
                             node->getOpCode().getName(),node,child->getOpCode().getName(),child))
      {
      TR::Node::recreate(node, TR::ILOpCode::getProperConversion(child->getFirstChild()->getDataType(), node->getDataType(), false));
      dumpOptDetails(s->comp(), "%s\n",node->getOpCode().getName());
      TR::Node *newChild = child->getFirstChild();
      if (isAbs)
         {
         TR::Node *abs = TR::Node::create(node, TR::ILOpCode::absOpCode(child->getFirstChild()->getDataType()), 1);
         abs->setAndIncChild(0, newChild);
         newChild = abs;
         }

      node->setAndIncChild(0, newChild);
      child->recursivelyDecReferenceCount();
      node->setFlags(0);
      return node;
      }

   if (!s->comp()->getOption(TR_DisableZonedToDFPReduction) &&
       s->cg()->supportsZonedDFPConversions() &&
       child->getOpCodeValue() == TR::zd2pd &&
       node->getDecimalFraction() == 0)
      {
      // pd2df/pd2dfAbs
      //    zd2pd
      //       zdX
      // to:
      // zd2df/zd2dfAbs
      //    zdX
      //
      TR::ILOpCodes convOp = TR::ILOpCode::getProperConversion(child->getFirstChild()->getDataType(), node->getDataType(), false); // isUnsigned=false
      if (isAbs)
         convOp = TR::ILOpCode::zonedToDFPAbsOp(child->getFirstChild()->getDataType(), node->getDataType());

      // do not need an intermediateTruncation check above because replaceNodeWithChild will take care of inserting a modPrec if needed
      if (convOp != TR::BadILOp &&
          performTransformation(s->comp(), "%sFold %s (0x%p) and child %s (0x%p) to ",s->optDetailString(),
            node->getOpCode().getName(),node,child->getOpCode().getName(),child))
         {
         TR::Node::recreate(node, convOp);
         dumpOptDetails(s->comp(), "%s\n",node->getOpCode().getName());
         node->setChild(0, s->replaceNodeWithChild(child, child->getFirstChild(), s->_curTree, block));
         node->setFlags(0);
         return node;
         }
      }

   if (!s->comp()->getOption(TR_DisableZonedToDFPReduction) &&
       s->cg()->supportsZonedDFPConversions() &&
       child->getOpCodeValue() == TR::pdSetSign &&
       child->isNonNegative() &&
       child->getFirstChild()->getOpCodeValue() == TR::zd2pd &&
       node->getDecimalFraction() == 0)
      {
      // pd2df
      //    pdSetSign >= 0
      //       zd2pd
      //          zdX
      //       iconst 0xf/0xc
      // to:
      // zd2dfAbs
      //    zdX
      //
      // Doesn't matter here if we have a pd2dfAbs or pd2df; the pdSetSign is redundant given the pd2dfAbs, and we'll end up with the same result anyway
      TR::ILOpCodes convOp = TR::ILOpCode::zonedToDFPAbsOp(child->getFirstChild()->getFirstChild()->getDataType(), node->getDataType());
      if (convOp != TR::BadILOp &&
          !child->hasIntermediateTruncation() &&
          performTransformation(s->comp(), "%sFold %s (0x%p) and child %s (0x%p) to ",s->optDetailString(),
            node->getOpCode().getName(),node,child->getOpCode().getName(),child))
         {
         int32_t childPrecision = child->getDecimalPrecision();
         TR::Node::recreate(node, convOp);    // pd2df -> zd2dfAbs (momentarily type incorrect but not exposed outside this simplifier)
         node->setFlags(0);
         dumpOptDetails(s->comp(), "%s\n",node->getOpCode().getName());
         // pd2df (zd2dfAbs)
         //    pdSetSign
         //       zd2pd
         //          zdX
         //       iconst sign
         // to:
         // pd2df (zd2dfAbs)
         //    zd2pd
         //       zdX
         // correctBCDPrecision=false trunc checks already done and top level prec is being maintained (false is needed so double replaceNodes work)
         node->setChild(0, s->replaceNodeWithChild(node->getFirstChild(), node->getFirstChild()->getFirstChild(), s->_curTree, block, false));
         // pd2df (zd2dfAbs)
         //    zd2pd
         //       zdX
         // to:
         // pd2df (zd2dfAbs)
         //    zdX
         node->setChild(0, s->replaceNodeWithChild(node->getFirstChild(), node->getFirstChild()->getFirstChild(), s->_curTree, block, false));
         if (childPrecision != node->getFirstChild()->getDecimalPrecision())
            {
            // if the original firstChild pdSetSign truncated then must retain this by inserting a zdModPrec
            // zd2dfAbs
            //    zdModPrec p=5
            //       zdX p=7
            //
            TR::ILOpCodes modifyPrecisionOp = TR::ILOpCode::modifyPrecisionOpCode(node->getFirstChild()->getDataType());
            TR_ASSERT(modifyPrecisionOp != TR::BadILOp,"no bcd modify precision opcode found\n");
            TR::Node *newNode = TR::Node::create(node->getFirstChild(), modifyPrecisionOp, 1);
            dumpOptDetails(s->comp()," create %s (%p) to correct precision to %d\n",newNode->getOpCode().getName(),newNode,childPrecision);
            newNode->setChild(0, node->getFirstChild());
            newNode->setDecimalPrecision(childPrecision);
            node->setAndIncChild(0, newNode);
            }
         return node;
         }
      }

   if (child->getOpCodeValue() == TR::pdSetSign &&
       child->isNonNegative() &&
       !child->hasIntermediateTruncation() &&
       node->getDecimalFraction() == 0)
      {
      // pd2df
      //    pdSetSign >= 0
      //       pdX
      //       iconst 0xf/0xc
      // to:
      // pd2dfAbs
      //    pdX
      //
      TR::ILOpCodes convOp = TR::ILOpCode::packedToDFPAbsOp(child->getFirstChild()->getDataType(), node->getDataType());
      if (convOp != TR::BadILOp &&
          performTransformation(s->comp(), "%sFold %s (0x%p) and child %s (0x%p) to ",s->optDetailString(),
            node->getOpCode().getName(),node,child->getOpCode().getName(),child))
         {
         int32_t childPrecision = child->getDecimalPrecision();
         TR::Node::recreate(node, convOp);    // pd2df -> pd2dfAbs
         node->setFlags(0);
         dumpOptDetails(s->comp(), "%s\n",node->getOpCode().getName());
         // pd2df (pd2dfAbs)
         //    pdSetSign
         //       pdX
         //       iconst sign
         // to:
         // pd2df (pd2dfAbs)
         //       pdX
         // correctBCDPrecision=false trunc checks already done and top level prec is being maintained (false is needed so double replaceNodes work)
         node->setChild(0, s->replaceNodeWithChild(node->getFirstChild(), node->getFirstChild()->getFirstChild(), s->_curTree, block, false));
         if (childPrecision != node->getFirstChild()->getDecimalPrecision())
            {
            // if the original firstChild pdSetSign truncated then must retain this by inserting a pdModPrec
            // pd2dfAbs
            //    zdModPrec p=5
            //       zdX p=7
            //
            TR::ILOpCodes modifyPrecisionOp = TR::ILOpCode::modifyPrecisionOpCode(node->getFirstChild()->getDataType());
            TR_ASSERT(modifyPrecisionOp != TR::BadILOp,"no bcd modify precision opcode found\n");
            TR::Node *newNode = TR::Node::create(node->getFirstChild(), modifyPrecisionOp, 1);
            dumpOptDetails(s->comp()," create %s (%p) to correct precision to %d\n",newNode->getOpCode().getName(),newNode,childPrecision);
            newNode->setChild(0, node->getFirstChild());
            newNode->setDecimalPrecision(childPrecision);
            node->setAndIncChild(0, newNode);
            }
         return node;
         }
      }

   return node;
   }

// Handles df2pd, dd2pd, de2pd
TR::Node *df2pdSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   // dd2pd over pd2dd: both can be cancelled, but must be a plain dd2pd (not a clean or setsign)
   TR::Node *child = node->getFirstChild();
   if (node->getOpCodeValue() != TR::ILOpCode::dfpToPackedCleanOp(child->getDataType(), node->getDataType()))
      {
      TR::Node *result = s->unaryCancelOutWithChild(node, child, s->_curTree, TR::ILOpCode::getProperConversion(node->getDataType(), child->getDataType(), false));
      if (result != NULL)
         return result;
      }

   return node;
   }

TR::Node *pdnegSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node *child = node->setChild(0,removeOperandWidening(node->getFirstChild(), node, block, s));

   TR::Node *result = NULL;
   if ((result = s->unaryCancelOutWithChild(node, child, s->_curTree, TR::ILOpCode::negateOpCode(node->getDataType()), false))) // anchorChildren=false
      return result;

   TR::ILOpCodes setSignOp = TR::ILOpCode::setSignOpCode(node->getDataType());
   if (setSignOp != TR::BadILOp &&
       NUM_DEFAULT_CHILDREN >= 2 &&
       child->hasKnownOrAssumedSignCode())
      {
      TR_RawBCDSignCode sign = child->getKnownOrAssumedSignCode();
      int32_t negatedSign = TR::DataType::getInvalidSignCode();
      switch (sign)
         {
         case raw_bcd_sign_0xf:
         case raw_bcd_sign_0xc:
            {
            negatedSign = TR::DataType::getPreferredMinusCode();
            }
            break;
         case raw_bcd_sign_0xd:
            {
            negatedSign = TR::DataType::getPreferredPlusCode();
            }
            break;
         default: break;
         }
      if (negatedSign != TR::DataType::getInvalidSignCode() &&
          performTransformation(s->comp(),"%sStrength reducing %s [" POINTER_PRINTF_FORMAT "] with known/assumed sign 0x%x child %s [" POINTER_PRINTF_FORMAT "] to ",
            s->optDetailString(), node->getOpCode().getName(),node,TR::DataType::getValue(sign),child->getOpCode().getName(),child))
         {
         TR::Node::recreate(node, setSignOp);
         dumpOptDetails(s->comp(),"%s 0x%x\n",node->getOpCode().getName(), negatedSign);
         node->setFlags(0);
         if (setSignOp == child->getOpCodeValue()) // if child is just a basic setSign operation too (pdSetSign,...) then it can be removed
            node->setChild(0, s->replaceNodeWithChild(child, child->getFirstChild(), s->_curTree, block));
         TR::Node *setSignValue = TR::Node::iconst(node, negatedSign);
         if (node->getNumChildren() == 2)
            node->setChild(1, s->replaceNode(node->getSecondChild(), setSignValue, s->_curTree));
         else
            node->setAndIncChild(1, setSignValue);
         node->setNumChildren(2);
         return node;
         }
      }
   return node;
   }

static TR::Node *foldSetSignIntoNode(TR::Node *setSign, bool setSignIsTheChild, TR::Node *other, bool removeSetSign, TR::Block *block, TR::Simplifier *s)
   {
   TR::Node *parent = setSignIsTheChild ? other : setSign;
   bool isShiftToShiftFold = setSign->getOpCode().isShift() && other->getOpCode().isShift();
   if (removeSetSign && isShiftToShiftFold)
      {
      TR_ASSERT(false,"shift to shift setSign folding not supported when removeSetSign=true : setSign %p other %p\n",setSign,other);
      return parent;
      }

   TR::Node *child = setSignIsTheChild ? setSign : other;
   // if the child is truncating and the parent widens and the setSign operation will be removed then any intermediate truncation will be lost so
   // the transformation would be invalid.
   if (removeSetSign &&
       parent->hasIntermediateTruncation())
      {
      if (s->trace())
         traceMsg(s->comp(),"disallow foldSetSignIntoNode of setSign -- %s (%p) with other node -- %s (%p) because child %p (prec %d) truncates and parent %p (prec %d) widens\n",
            setSign->getOpCode().getName(),setSign,other->getOpCode().getName(),other,child,child->getDecimalPrecision(),parent,parent->getDecimalPrecision());
      return parent;
      }

   bool removeOtherPaddingAddress = false;
   TR::Node *paddingAddress = NULL;
   TR::Node *otherPaddingAddress = NULL;
   if (paddingAddress)
      {
      otherPaddingAddress = NULL;
      if (otherPaddingAddress)
         removeOtherPaddingAddress = true;
      }
   else
      {
      paddingAddress = NULL;
      }

   TR::ILOpCodes setSignOp = TR::BadILOp;

   setSignOp = TR::ILOpCode::setSignVersionOfOpCode(other->getOpCodeValue());

   TR_ASSERT(setSignOp != TR::BadILOp,"could not find setSignOp for node %p\n",other);

   int32_t convertedSetSign = TR::DataType::getInvalidSignCode();
   int32_t originalSetSignIndex = TR::ILOpCode::getSetSignValueIndex(setSign->getOpCodeValue());
   if (setSign->getChild(originalSetSignIndex)->getOpCode().isLoadConst())
      {
      int32_t originalSetSign = setSign->getChild(originalSetSignIndex)->get32bitIntegralValue();
      convertedSetSign = TR::DataType::convertSignEncoding(setSign->getDataType(), parent->getDataType(), originalSetSign);
      TR_ASSERT(convertedSetSign != TR::DataType::getInvalidSignCode(),"could not convert sign encoding of 0x%x from type %s to type %s\n",originalSetSign,setSign->getDataType().toString(),parent->getDataType().toString());
      }
   if (convertedSetSign != TR::DataType::getInvalidSignCode() &&
       setSign->getReferenceCount() == 1 &&
       other->getReferenceCount() == 1 &&
       setSignOp != TR::BadILOp &&
       performTransformation(s->comp(), "%sFold %s [" POINTER_PRINTF_FORMAT "] into setsign %s %s [" POINTER_PRINTF_FORMAT "] and create new ",
         s->optDetailString(),other->getOpCode().getName(),other,setSignIsTheChild?"child":"parent",setSign->getOpCode().getName(),setSign))
      {
      int32_t newSetSignIndex = TR::ILOpCode::getSetSignValueIndex(setSignOp);
      TR::Node *newNode = NULL;
      TR::Node *src = NULL;
      if (removeSetSign || !setSignIsTheChild)
         {
         src = setSignIsTheChild ? setSign->getFirstChild() : other->getFirstChild();
         }
      else
         {
         // In this case we are transforming:
         // pd2zdsls
         //    pdshlSetSign
         //       x
         //       shift
         //       sign
         // to:
         // pd2zdslsSetSign
         //    pdshlSetSign
         //       x
         //       shift
         //       sign
         //    newSign
         // So we want the src to be the setSign node itself
         src = setSign;
         }
      TR::Node *newSetSign = TR::Node::iconst(src, convertedSetSign);
      newSetSign->incReferenceCount(); // all the other nodes added to newNode are existing nodes so inc'ing here allows all children refCounts to be corrected later
      if (newSetSignIndex == 1)      // child 2
         {
         if (other->getOpCode().isConversion() &&
             other->getFirstChild()->getType().isUnicodeSeparateSign() &&
             other->getSecondChild()->getDataType() == TR::Address)
            {
            // Transform:
            //
            //     pdSetSign - node
            //       udsx2pd - child
            //          x
            //          signAddrForCompareAndSet (could be '-' or '+')
            //       iconst sign
            //
            // to:
            //
            //    udsx2pdSetSign - node
            //       x
            //       iconst sign
            //
            other->getSecondChild()->recursivelyDecReferenceCount();
            if (paddingAddress)
               newNode = TR::Node::create(setSignOp, 3,
                                         src,
                                         newSetSign,
                                         paddingAddress);
            else
               newNode = TR::Node::create(setSignOp, 2,
                                         src,
                                         newSetSign);
            }
         else if (other->getNumChildren() == 1)
            {
            // Transform:
            //
            //    pd2zdsls - node
            //       pdSetSign - child
            //          x
            //          iconst sign
            //
            // to:
            //
            //    pd2zdslsSetSign - node
            //       x
            //       iconst sign
            //

            if (paddingAddress)
               newNode = TR::Node::create(setSignOp, 3,
                                         src,
                                         newSetSign,
                                         paddingAddress);
            else
               newNode = TR::Node::create(setSignOp, 2,
                                         src,
                                         newSetSign);
            }
         else
            {
            TR_ASSERT(false,"unexpected parent/child combination in foldSetSignIntoNode for newSetSignIndex == 1\n");
            }
         }
      else if (newSetSignIndex == 2) // child 3
         {
         // Transform:                      |  Transform:                                             | Transform (a removeSetSign=false case):
         //                                 |                                                         |
         //    pdshl - node                 |      pd2udsl - node                                     |       pd2udsl - node
         //       pdSetSign - child         |         pdSetSign - child                               |          pdshlSetSign - child
         //          x                      |            x                                            |             x
         //          iconst sign            |            iconst sign                                  |             iconst shift
         //       iconst shift              |         signAddrForCompareAndSet (could be '-' or '+')  |             iconst sign
         // to:                             |   to:                                                   |          signAddrForCompareAndSet (could be '-' or '+')
         //                                 |                                                         |      to:
         //    pdshlSetSign - node          |      pd2udslSetSign - node                              |       pd2udslSetSign - node
         //          x                      |            x                                            |          pdshlSetSign
         //          iconst shift           |            newSignAddrForSet (from the convertedSign)   |             x
         //          iconst convertedSign   |            iconst convertedSign                         |             iconst shift
         //                                 |                                                         |             iconst sign
         //                                 |                                                         |          newSignAddrForSet (from the convertedSign)
         //                                 |                                                         |          iconst convertedSign
         //                                 |                                                         |
         //

         TR::Node *newSecondChild =  NULL;
         if (isShiftToShiftFold)
            {
            newSecondChild = other->getSecondChild();
            }
         else if (setSign->getOpCode().isShift())
            {
            newSecondChild = setSign->getSecondChild();
            }
         else if (other->getOpCode().isShift())
            {
            newSecondChild = other->getSecondChild();
            }
         else
            {
            TR_ASSERT(false,"unexpected parent/child combination in foldSetSignIntoNode for newSetSignIndex == 2\n");
            }
         if (newSecondChild)
            {
            if (paddingAddress)
               newNode = TR::Node::create(setSignOp, 4,
                                         src,
                                         newSecondChild,
                                         newSetSign,
                                         paddingAddress);
            else
               newNode = TR::Node::create(setSignOp, 3,
                                         src,
                                         newSecondChild,
                                         newSetSign);

            }
         }
      else if (newSetSignIndex == 3) // child 4
         {
         // Transform:
         //
         //    pdshr - node
         //       pdSetSign - child
         //          x
         //          iconst sign
         //       iconst shift
         //       iconst round
         //
         // to:
         //
         //    pdshrSetSign - node
         //          x
         //          iconst shift
         //          iconst round
         //          iconst sign
         //
         TR::Node  *newSecondChild = NULL;
         TR::Node  *newThirdChild = NULL;
         if (isShiftToShiftFold)
            {
            newSecondChild = other->getSecondChild();
            newThirdChild  = other->getThirdChild();
            }
         else if (setSign->getOpCode().isShift())
            {
            newSecondChild = setSign->getSecondChild();
            newThirdChild  = setSign->getThirdChild();
            }
         else if (other->getOpCode().isShift())
            {
            newSecondChild = other->getSecondChild();
            newThirdChild  = other->getThirdChild();
            }
         else
            {
            TR_ASSERT(false,"unexpected parent/child combination in foldSetSignIntoNode for newSetSignIndex == 3\n");
            }
         if (newSecondChild && newThirdChild)
            {
            if (paddingAddress)
               newNode = TR::Node::create(setSignOp, 5,
                                         src,
                                         newSecondChild,             // shiftAmount
                                         newThirdChild,              // roundAmount
                                         newSetSign,
                                         paddingAddress);
            else
               newNode = TR::Node::create(setSignOp, 4,
                                         src,
                                         newSecondChild,             // shiftAmount
                                         newThirdChild,              // roundAmount
                                         newSetSign);
            }
         }
      else
         {
         TR_ASSERT(false,"unexpected newSetSignIndex of %d\n",newSetSignIndex);
         }

      if (newNode)
         {
         // TODO: call/transfer newNode->setKnownSignCode() when the setSign value is a constant
         // (and in ilgen call setKnownSignCode() on the xSetSign node too)
         dumpOptDetails(s->comp(), "%s [" POINTER_PRINTF_FORMAT "] with decimalPrecision of ", newNode->getOpCode().getName(),newNode);
         newNode->incReferenceCount();
         // All the children on newNode were existing nodes except for newSetSign and were therefore incremented one extra time each when creating newNode above
         // and newSetSign was explicitly inc'ed
         for (int32_t i=0; i < newNode->getNumChildren(); i++)
            newNode->getChild(i)->decReferenceCount();
         stopUsingSingleNode(other, removeOtherPaddingAddress, s); // removePadding=removeOtherPaddingAddress
         if (removeSetSign)
            {
            // the newNode is replacing both the original parent/child so its precision must be that of the parent's
            // pdSetSign p=5 <- parent (and setSign)
            //    pdshl p=x  <- other
            // into
            // pdshlSetSign p=5 <- newNode
            dumpOptDetails(s->comp(), "%d from parent node %s [" POINTER_PRINTF_FORMAT "] and paddingAddress [" POINTER_PRINTF_FORMAT "]\n",
               parent->getDecimalPrecision(), parent->getOpCode().getName(),parent,paddingAddress);
            newNode->setDecimalPrecision(parent->getDecimalPrecision());
            setSign->getChild(originalSetSignIndex)->recursivelyDecReferenceCount();
            stopUsingSingleNode(setSign, false, s); // removePadding=false, paddingAddress is still attached to the newNode
            return s->simplify(newNode, block);
            }
         else
            {
            dumpOptDetails(s->comp(), "%d from other node %s [" POINTER_PRINTF_FORMAT "] and paddingAddress [" POINTER_PRINTF_FORMAT "]\n",
               other->getDecimalPrecision(), other->getOpCode().getName(),other,paddingAddress);
            newNode->setDecimalPrecision(other->getDecimalPrecision());
            if (setSignIsTheChild)
               {
               // pd2zdsls p=5 <- other
               //    pdshlSetSign p=x
               // into
               // pdzdslsSetSign p=5 <- newNode
               //    pdshlSetSign p=x
               return newNode;
               }
            else
               {
               // pdshlSetSign p=x
               //    zdsls2pd p=5 <- other (and src)
               // into
               // pdshlSetSign p=x
               //    zdsls2pdSetSign p=5 <- newNode
               parent->setChild(0, s->simplify(newNode, block));
               return parent;
               }
            }
         }
      else
         {
         return parent;
         }
      }
   else
      {
      return parent;
      }
   }

static TR::Node *foldAndReplaceDominatedSetSign(TR::Node *setSign, bool setSignIsTheChild, TR::Node *other, TR::Block * block, TR::Simplifier * s)
   {
   // Attempt to fold a child (or parent) setSign operation into a new setSign version of the parent (or child) and leave the original setSign operation in place
   // (because for example the child setSign also performs some other operation such as a shift that cannot be folded into the parent too).
   // If this is folding is successful then change the original child (or parent) setSign value to a new value that will be less expensive to evaluate.
   //
   // For example transform (setSignIsTheChild=true)
   //
   //      pd2zdsls        - other
   //         pdshlSetSign - setSign
   //            x
   //            iconst shift
   //            iconst sign
   //
   //     to:
   //
   //      pd2zdslsSetSign - newNode
   //         pdshlSetSign - setSign
   //            x
   //            iconst shift
   //            iconst TR::DataType::getIgnoredSignCode()
   //         iconst zdslsVersionOf(sign)
   //
   // For example transform (setSignIsTheChild=false)
   //
   //      pdshrSetSign - setSign
   //         udst2pd   - other
   //            x
   //            iconst signA
   //         iconst shift
   //         iconst signB
   //
   //     to:
   //
   //      pdshrSetSign       - setSign
   //         udst2pdSetSign  - newChild
   //            x
   //            iconst TR::DataType::getIgnoredSignCode()
   //         iconst shift
   //         iconst signB
   //
   int32_t newChildSetSignValue = TR::DataType::getIgnoredSignCode();
   TR::Node *parent = setSignIsTheChild ? other : setSign;
   TR::Node *child = parent->getFirstChild();
   // In the special case of an odd shift right set sign as a parent of a left shift then have the left shift
   // child clear the space for the shift digits and setSign for the parent all in one operation
   // Instead of:
   //    ClearLowBytes     : MVI 0x00 from pdshl
   //    SetSignLow        : MVI 0x0f from pdshr
   //    RightShift        : MVO
   // do
   //    ClearAndSetSign   : MVI 0x0f from pdshl
   //    RightShift        : MVO
   if (parent->getOpCode().isSetSign() &&
       parent->getOpCode().isRightShift() &&
       parent->getSetSignValueNode()->getOpCode().isLoadConst() &&
       parent->getSecondChild()->getOpCode().isLoadConst() &&
       (parent->getSecondChild()->get64bitIntegralValue() & 0x1) != 0 &&   // odd right shift + setsign
       child->getOpCode().isLeftShift() &&
       child->getSecondChild()->getOpCode().isLoadConst() &&
       child->getSecondChild()->get64bitIntegralValue() == 1)     // left shift by 1 - could be expanded for larger shift amounts when a padding address is used for left shifts
      {
      newChildSetSignValue = parent->getSetSignValueNode()->get32bitIntegralValue();
      }
   if (setSignIsTheChild || TR::ILOpCode::setSignVersionOfOpCode(child->getOpCodeValue()) != TR::BadILOp)
      {
      parent = foldSetSignIntoNode(setSign, setSignIsTheChild, other, false /* !removeSetSign */, block, s);
      child = parent->getFirstChild();
      if (parent->getOpCode().isSetSign() &&
          child->getOpCode().isConversion() &&
          child->getOpCode().isSetSign() &&
          child->getFirstChild()->getType().isZonedSeparateSign())
         {
         // the ignored set sign conversion from a zoned sep sign type has to verify if the input sign is valid
         // so in the case below leave the folded signA as is after foldSetSignIntoNode
         //    pdshrSetSign
         //       zdsls2pdSetSign
         //          zdsls_x
         //          iconst signA
         //       iconst shift
         //       iconst signB
         //
         if (s->trace())
            traceMsg(s->comp(),"disallow setting ignored setsign value on dominated %s [" POINTER_PRINTF_FORMAT "] so sign verification can be skipped\n",
               child->getOpCode().getName(),child);
         return parent;
         }
      else if (parent->getOpCode().isSetSign() && parent->getFirstChild()->getOpCode().isSetSign())
         {
         int32_t childSetSignIndex = TR::ILOpCode::getSetSignValueIndex(child->getOpCodeValue());
         if (child->getReferenceCount() == 1 &&
             child->getChild(childSetSignIndex)->getOpCode().isLoadConst() &&
             performTransformation(s->comp(), "%sReplace %s [" POINTER_PRINTF_FORMAT "] dominated %s [" POINTER_PRINTF_FORMAT "] setSignValue node [" POINTER_PRINTF_FORMAT "] with new ignored setSignValue %d ",
               s->optDetailString(),parent->getOpCode().getName(),parent,child->getOpCode().getName(),child, child->getChild(childSetSignIndex),newChildSetSignValue))
            {
            TR::Node *newSetSign = TR::Node::iconst(child, newChildSetSignValue);
            newSetSign->incReferenceCount();
            child->getChild(childSetSignIndex)->recursivelyDecReferenceCount();
            child->setChild(childSetSignIndex, newSetSign);
            child->resetSignState();
            dumpOptDetails(s->comp(), " node [" POINTER_PRINTF_FORMAT "]\n", newSetSign);
            }
         }
      }
   return parent;
   }

static TR::Node *createSetSignForKnownSignChild(TR::Node *node, TR::Block * block, TR::Simplifier * s)
   {
   TR::Node *newNode = NULL;
   TR::Node *child = node->getFirstChild();
   if (node->getReferenceCount() == 1 &&
       child->getReferenceCount() == 1 &&
       child->alwaysGeneratesAKnownPositiveCleanSign())
      {
      bool isShiftToShiftFold = node->getOpCode().isShift() && child->getOpCode().isShift();
      if (isShiftToShiftFold)
         {
         TR_ASSERT(false,"shift to shift setSign folding not supported : node %p child %p\n",node,child);
         return node;
         }
      TR::ILOpCodes setSignOp = TR::ILOpCode::setSignVersionOfOpCode(node->getOpCodeValue());
      if (setSignOp != TR::BadILOp &&
          performTransformation(s->comp(), "%sFold alwaysGeneratesAKnownPositiveCleanSign child %s [" POINTER_PRINTF_FORMAT "] into %s [" POINTER_PRINTF_FORMAT "] and create new ",
            s->optDetailString(),child->getOpCode().getName(),child,node->getOpCode().getName(),node))
         {
         int32_t newSetSignIndex = TR::ILOpCode::getSetSignValueIndex(setSignOp);
         int32_t convertedSetSign = TR::DataType::convertSignEncoding(child->getDataType(), node->getDataType(), TR::DataType::getPreferredPlusCode());
         TR::Node *newSetSign = TR::Node::iconst(node, convertedSetSign);
         if (newSetSignIndex == 2)
            {
            TR::Node *newSecondChild = NULL;
            if (node->getOpCode().isShift())
               {
               // Transform:
               //
               //    pdshl - node
               //       ud2pd - child
               //          x
               //       iconst shift
               // to:
               //
               //    pdshlSetSign - newNode
               //       ud2pd
               //          x
               //       iconst shift
               //       iconst convertedSign
               //
               newSecondChild = node->getSecondChild();
               }
            else
               {
               TR_ASSERT(false,"unexpected parent/child combination in pd2udslSimplifier for newSetSignIndex == 2\n");
               }
            if (newSecondChild)
               {
               newNode = TR::Node::create(setSignOp, 3,
                                         child,
                                         newSecondChild,
                                         newSetSign);
               }
            }
         else if (newSetSignIndex == 3)
            {
            // Transform:
            //
            //    pdshr - node
            //       ud2pd - child
            //          x
            //       iconst shift
            //       iconst round
            // to:
            //
            //    pdshrSetSign - newNode
            //       ud2pd - child
            //          x
            //       iconst shift
            //       iconst round
            //       iconst convertedSign
            //
            TR_ASSERT(node->getNumChildren() == 3,"expecting three children on the shift node (%p) and not %d children\n",node,node->getNumChildren());
            newNode = TR::Node::create(setSignOp, 4,
                                      child,
                                      node->getSecondChild(),             // shiftAmount
                                      node->getThirdChild(),              // roundAmount
                                      newSetSign);
            }
         else
            {
            TR_ASSERT(false,"unexpected newSetSignIndex of %d\n",newSetSignIndex);
            }
         if (newNode)
            {
            dumpOptDetails(s->comp(), "%s [" POINTER_PRINTF_FORMAT "] with convertedSetSign of 0x%x\n", newNode->getOpCode().getName(),newNode,convertedSetSign);
            newNode->incReferenceCount();
            newNode->setDecimalPrecision(node->getDecimalPrecision());
            // All the children up to but not including the setSignValue on newNode were existing nodes and were therefore incremented one extra time each when creating newNode above
            for (int32_t i=0; i < newNode->getNumChildren()-1; i++)
               newNode->getChild(i)->decReferenceCount();
            stopUsingSingleNode(node, true, s); // removePadding=true
            return newNode;
            }
         else
            {
            return node;
            }
         }
      }
   return node;
   }

// Handles the following:
//
// ifdecmpeq
// ifdecmpne
// ifdecmplt
// ifdecmpge
// ifdecmpgt
// ifdecmple
//
// ifddcmpeq
// ifddcmpne
// ifddcmplt
// ifddcmpge
// ifddcmpgt
// ifddcmple
//
// ifdfcmpeq
// ifdfcmpne
// ifdfcmplt
// ifdfcmpge
// ifdfcmpgt
// ifdfcmple

TR::Node *ifDFPCompareSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   if (removeIfToFollowingBlock(node, block, s) == NULL)
      return NULL;
   simplifyChildren(node, block, s);

   TR::Node *firstChild = node->getFirstChild();
   firstChild = node->setChild(0,removeUnnecessaryDFPClean(firstChild, node, block, s));

   TR::Node *secondChild = node->getSecondChild();
   secondChild = node->setChild(1,removeUnnecessaryDFPClean(secondChild, node, block, s));

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);

   TR::ILOpCodes childOp = firstChild->getOpCodeValue();

   TR::Node *constChild = secondChild;
   if (secondChild->getOpCode().isConversion() && secondChild->getFirstChild()->getOpCode().isLoadConst())
      constChild = secondChild->getFirstChild();

   return node;
   }

// Handles df2dd, df2de, dd2df, dd2de, de2df, de2dd
TR::Node *dfp2dfpSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   return node;
   }

// Handles df2pdSetSign, dd2pdSetSign, de2pdSetSign
TR::Node *df2pdSetSignSimplifier(TR::Node *node, TR::Block *block, TR::Simplifier *s)
   {
   simplifyChildren(node, block, s);

   TR::Node *child = node->getFirstChild();
   child = node->setChild(0,removeUnnecessaryDFPClean(child, node, block, s));

   return node;
   }

// Handles i2df, iu2df, l2df, lu2df, i2dd etc, and i2de etc.
TR::Node *int2dfpSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node *child = node->getFirstChild();

   // i2dd over dd2i: both can be cancelled
   TR::Node *result = s->unaryCancelOutWithChild(node, child, s->_curTree, TR::ILOpCode::getProperConversion(node->getDataType(), child->getDataType(), false));
   if (result != NULL)
      return result;

   return node;
   }

// Handles df2i, df2iu, df2l, df2lu, dd2i etc, and de2i etc.
TR::Node *dfp2intSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node *child = node->getFirstChild();

   // dd2i over i2dd: both can be cancelled
   if (node->getDataType().getMaxPrecisionFromType() <= child->getDataType().getMaxPrecisionFromType())
      {
      TR::Node *result = s->unaryCancelOutWithChild(node, child, s->_curTree, TR::ILOpCode::getProperConversion(node->getDataType(), child->getDataType(), false));
      if (result != NULL)
         return result;
      }

   return node;
   }

TR::Node *dfpShiftLeftSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node *firstChild = node->getFirstChild();
   TR::ILOpCodes childOp = firstChild->getOpCodeValue();

   return node;
   }

TR::Node *dfpShiftRightSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node *firstChild = node->getFirstChild();
   TR::ILOpCodes childOp = firstChild->getOpCodeValue();

   return node;
   }

TR::Node *dfpModifyPrecisionSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node *firstChild = node->getFirstChild();
   TR::ILOpCodes childOp = firstChild->getOpCodeValue();

#ifdef SUPPORT_DFP
   // Two consecutive modPs: get rid of the lower one
   if (firstChild->getOpCodeValue() == node->getOpCodeValue() &&
      performTransformation(s->comp(), "%sFold %s (0x%p) <p=%d> and child %s (0x%p) <p=%d> to %s (0x%p) <p=%d>\n",s->optDetailString(),
                            node->getOpCode().getName(),node,node->getDFPPrecision(),firstChild->getOpCode().getName(),firstChild,firstChild->getDFPPrecision(),
                            node->getOpCode().getName(),node,std::min(node->getDFPPrecision(),firstChild->getDFPPrecision())))
      {
      node->setDFPPrecision(std::min(node->getDFPPrecision(),firstChild->getDFPPrecision()));
      node->setChild(0, firstChild->getFirstChild());
      firstChild->decReferenceCount();
      }
#endif

   return node;
   }

// Handles dfabs, ddabs, deabs, dfSetNegative, ddSetNegative, deSetNegative
TR::Node *dfpSetSignSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node *redundantParent = node;
   TR::Node *redundantDescendant = getRedundantDFPSetSignDescendant(node->getFirstChild(), &redundantParent);
   if (redundantDescendant != NULL &&
       performTransformation(s->comp(), "%sRemove redundant setsign op %s (0x%p) below ancestor %s (0x%p)",s->optDetailString(),
                             redundantDescendant->getOpCode().getName(),redundantDescendant,node->getOpCode().getName(),node))
      {
      // There's nothing in between this node and the lower node that needs the sign to be correct, so the
      // lower node can be removed
      redundantParent->setChild(0, redundantDescendant->getFirstChild());
      redundantDescendant->decReferenceCount();
      }

   if (!node->isNonNegative() &&
       node->getOpCode().isAbs() &&
       performTransformation(s->comp(),"%sSet X>=0 flag on %s [" POINTER_PRINTF_FORMAT "] due to abs operation\n",s->optDetailString(),node->getOpCode().getName(),node))
      {
      node->setIsNonNegative(true);
      }

   return node;
   }

TR::Node *dfpFloorSimplifier(TR::Node *node, TR::Block *block, TR::Simplifier *s)
   {
   simplifyChildren(node, block, s);
   if (!node->isNonNegative() &&
       node->getFirstChild()->isNonNegative() &&
       performTransformation(s->comp(),"%sSet X>=0 flag on %s [" POINTER_PRINTF_FORMAT "] due to child X>=0\n",s->optDetailString(),node->getOpCode().getName(),node))
      {
      node->setIsNonNegative(true);
      }
   return node;
   }

TR::Node *dfpAddSimplifier(TR::Node *node, TR::Block *block, TR::Simplifier *s)
   {
   simplifyChildren(node, block, s);
   propagateNonNegativeFlagForArithmetic(node, s);
   return node;
   }

TR::Node *dfpMulSimplifier(TR::Node *node, TR::Block *block, TR::Simplifier *s)
   {
   simplifyChildren(node, block, s);
   propagateNonNegativeFlagForArithmetic(node, s);
   return node;
   }

TR::Node *dfpDivSimplifier(TR::Node *node, TR::Block *block, TR::Simplifier *s)
   {
   simplifyChildren(node, block, s);
   propagateNonNegativeFlagForArithmetic(node, s);
   return node;
   }

