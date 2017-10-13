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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "omrport.h"

#include "optimizer/Simplifier.hpp"
#include "optimizer/J9SimplifierHelpers.hpp"
#include "optimizer/J9SimplifierHandlers.hpp"

#include "codegen/CodeGenerator.hpp"           // for CodeGenerator
#include "compile/Compilation.hpp"             // for Compilation, comp
#include "il/Block.hpp"                        // for Block
#include "il/Node.hpp"                         // for Node, etc
#include "il/Node_inlines.hpp"                 // for Node::getFirstChild, etc
#include "optimizer/Optimization_inlines.hpp"

bool
J9::Simplifier::isLegalToUnaryCancel(TR::Node *node, TR::Node *firstChild, TR::ILOpCodes opcode)
   {
   if (!OMR::Simplifier::isLegalToUnaryCancel(node, firstChild, opcode))
      return false;

   if (node->getOpCode().isConversionWithFraction() &&
       firstChild->getOpCode().isConversionWithFraction() &&
       (node->getDecimalFraction() != firstChild->getDecimalFraction()))
      {
      // illegal to fold a pattern like:
      // pd2f  frac=5
      //    f2pd frac=0
      //       f
      // to just 'f' because the extra digits that should be introduced by 
      // the frac=5 in the parent will be lost
      if (trace())
         traceMsg(comp(),"disallow unaryCancel of node %p and firstChild %p due to mismatch of decimal fractions (%d != %d)\n",
                 node,firstChild,node->getDecimalFraction(),firstChild->getDecimalFraction());
      return false;
      }

   if (firstChild->getOpCodeValue() == opcode &&
       node->getType().isBCD() && 
       firstChild->getType().isBCD() && 
       firstChild->getFirstChild()->getType().isBCD() &&
       node->hasIntermediateTruncation())
      {
      // illegal to fold if there is an intermediate (firstChild) truncation:
      // zd2pd p=4         0034
      //    pd2zd p=2        34
      //       pdx p=4     1234
      // if folding is performed to remove zd2pd/pd2zd then the result will 
      // be 1234 instead of 0034
      if (trace())
         traceMsg(comp(),"disallow unaryCancel of node %p and firstChild %p due to intermediate truncation of node\n",node,firstChild);
      return false;
      }
   else if (firstChild->getOpCodeValue() == opcode && 
            node->getType().isBCD() && 
            !firstChild->getType().isBCD())
      {
      // illegal to fold an intermediate truncation:
      // dd2zd p=20 srcP=13
      //   zd2dd (no p specifed, but by data type, must be <= 16 and by srcP must be <= 13)
      //     zdX p=20
      // Folding gives an incorrect result if either srcP or the implied p of the zd2dd is 
      // less than p on the dd2zd
      int32_t nodeP = node->getDecimalPrecision();
      int32_t childP = TR::DataType::getMaxPackedDecimalPrecision();
      int32_t grandChildP = firstChild->getFirstChild()->getDecimalPrecision();

      if (node->hasSourcePrecision())
         childP = node->getSourcePrecision();
      else if (firstChild->getDataType().canGetMaxPrecisionFromType())
         childP = firstChild->getDataType().getMaxPrecisionFromType();

      if (childP < nodeP && childP < grandChildP)
         {
         if (trace())
            traceMsg(comp(),"disallow unaryCancel of node %p and firstChild %p due to intermediate truncation of node\n",node,firstChild);
         return false;
         }
      }
   else if (firstChild->getOpCodeValue() == opcode && 
            !node->getType().isBCD() && 
            !firstChild->getType().isBCD())
      {
      // illegal to fold an intermediate truncation:
      // dd2l
      //   l2dd
      //     lX
      // Folding could give an incorrect result because the max precision of a dd is 16 
      // and the max precision of an l is 19
      if (node->getDataType().canGetMaxPrecisionFromType() && 
          firstChild->getDataType().canGetMaxPrecisionFromType() &&
          node->getDataType().getMaxPrecisionFromType() > firstChild->getDataType().getMaxPrecisionFromType())
         {
         if (trace())
            traceMsg(comp(),"disallow unaryCancel of node %p and firstChild %p due to intermediate truncation of node\n",node,firstChild);
         return false;
         }
      }

   return true;
   }

TR::Node *
J9::Simplifier::unaryCancelOutWithChild(TR::Node *node, TR::Node *firstChild, TR::TreeTop *anchorTree, TR::ILOpCodes opcode, bool anchorChildren)
   {
   TR::Node *grandChild = OMR::Simplifier::unaryCancelOutWithChild(node, firstChild, anchorTree, opcode, anchorChildren);
   if (grandChild == NULL)
      return NULL;

   TR_RawBCDSignCode alwaysGeneratedSign = comp()->cg()->alwaysGeneratedSign(node);
   if (node->getType().isBCD() &&
       grandChild->getType().isBCD() &&
       (node->getDecimalPrecision() != grandChild->getDecimalPrecision() || alwaysGeneratedSign != raw_bcd_sign_unknown))
      {
      // must maintain the top level node's precision when replacing with the grandchild
      // (otherwise if the parent of the node is call it will pass a too small or too big value)
      TR::Node *origOrigGrandChild = grandChild;
      if (node->getDecimalPrecision() != grandChild->getDecimalPrecision())
         {
         TR::Node *origGrandChild = grandChild;
         grandChild = TR::Node::create(TR::ILOpCode::modifyPrecisionOpCode(grandChild->getDataType()), 1, origGrandChild);
         origGrandChild->decReferenceCount(); // inc'd an extra time when creating modPrecision node above
         grandChild->incReferenceCount();
         grandChild->setDecimalPrecision(node->getDecimalPrecision());
         dumpOptDetails(comp(), "%sCreate %s [" POINTER_PRINTF_FORMAT "] to reconcile precision mismatch between node %s [" POINTER_PRINTF_FORMAT "] grandChild %s [" POINTER_PRINTF_FORMAT "] (%d != %d)\n",
               optDetailString(),
               grandChild->getOpCode().getName(),
               grandChild,
               node->getOpCode().getName(),
               node,
               origOrigGrandChild->getOpCode().getName(),
               origOrigGrandChild,
               node->getDecimalPrecision(),
               origOrigGrandChild->getDecimalPrecision());
         }
      // if the top level was always setting a particular sign code (e.g. ud2pd) then must maintain this side-effect here when cancelling
      if (alwaysGeneratedSign != raw_bcd_sign_unknown)
         {
         TR::Node *origGrandChild = grandChild;
         TR::ILOpCodes setSignOp = TR::ILOpCode::setSignOpCode(grandChild->getDataType());
         TR_ASSERT(setSignOp != TR::BadILOp,"could not find setSignOp for type %s on %s (%p)\n",
               grandChild->getDataType().toString(),grandChild->getOpCode().getName(),grandChild);
         grandChild = TR::Node::create(setSignOp, 2,
               origGrandChild,
               TR::Node::iconst(origGrandChild, TR::DataType::getValue(alwaysGeneratedSign)));
         origGrandChild->decReferenceCount(); // inc'd an extra time when creating setSign node above
         grandChild->incReferenceCount();
         grandChild->setDecimalPrecision(origGrandChild->getDecimalPrecision());
         dumpOptDetails(comp(), "%sCreate %s [" POINTER_PRINTF_FORMAT "] to preserve setsign side-effect between node %s [" POINTER_PRINTF_FORMAT "] grandChild %s [" POINTER_PRINTF_FORMAT "] (sign=0x%x)\n",
               optDetailString(),
               grandChild->getOpCode().getName(),
               grandChild,
               node->getOpCode().getName(),
               node,
               origOrigGrandChild->getOpCode().getName(),
               origOrigGrandChild,
               TR::DataType::getValue(alwaysGeneratedSign));
         }
      }
   else if (node->getType().isDFP() && firstChild->getType().isBCD())
      {
      // zd2dd
      //   dd2zd p=12 srcP=13
      //     ddX (p possibly unknown but <= 16)
      // Folding gives an incorrect result if the truncation on the dd2zd isn't preserved
      int32_t nodeP = TR::DataType::getMaxPackedDecimalPrecision();
      int32_t childP = firstChild->getDecimalPrecision();
      int32_t grandChildP = TR::DataType::getMaxPackedDecimalPrecision();

      if (node->getDataType().canGetMaxPrecisionFromType())
         {
         nodeP = node->getDataType().getMaxPrecisionFromType();
         grandChildP = nodeP;
         }
      if (firstChild->hasSourcePrecision())
         grandChildP = firstChild->getSourcePrecision();

      if (childP < nodeP && childP < grandChildP)
         {
         TR::Node *origOrigGrandChild = grandChild;
         TR::Node *origGrandChild = grandChild;
         grandChild = TR::Node::create(TR::ILOpCode::modifyPrecisionOpCode(grandChild->getDataType()), 1, origGrandChild);
         origGrandChild->decReferenceCount(); // inc'd an extra time when creating modPrecision node above
         grandChild->incReferenceCount();
         grandChild->setDFPPrecision(childP);
         dumpOptDetails(comp(), "%sCreate %s [" POINTER_PRINTF_FORMAT "] to reconcile precision mismatch between node %s [" POINTER_PRINTF_FORMAT "] grandChild %s [" POINTER_PRINTF_FORMAT "] (%d != %d)\n",
               optDetailString(),
               grandChild->getOpCode().getName(),
               grandChild,
               node->getOpCode().getName(),
               node,
               origOrigGrandChild->getOpCode().getName(),
               origOrigGrandChild,
               nodeP,
               childP);
         }
      }

   return grandChild;
   }

bool
J9::Simplifier::isRecognizedPowMethod(TR::Node *node)
   {
   TR::Symbol *symbol = node->getSymbol();
   TR::MethodSymbol *methodSymbol = symbol ? symbol->castToMethodSymbol() : NULL;
   return (methodSymbol &&
           (methodSymbol->getRecognizedMethod() == TR::java_lang_Math_pow ||
            methodSymbol->getRecognizedMethod() == TR::java_lang_StrictMath_pow));
   }

bool
J9::Simplifier::isRecognizedAbsMethod(TR::Node * node)
   {
   TR::Symbol *symbol = node->getSymbol();
   TR::MethodSymbol *methodSymbol = symbol ? symbol->castToMethodSymbol() : NULL;
   return  (methodSymbol &&
            (methodSymbol->getRecognizedMethod() == TR::java_lang_Math_abs_D ||
             methodSymbol->getRecognizedMethod() == TR::java_lang_Math_abs_F ||
             methodSymbol->getRecognizedMethod() == TR::java_lang_Math_abs_I));
   }

TR::Node *
J9::Simplifier::foldAbs(TR::Node *node)
   {
   TR::Node * childNode = NULL;

   if (node->getNumChildren() == 1)
      childNode = node->getFirstChild();
   else if (node->getNumChildren() == 2)
      {
      TR_ASSERT(node->getFirstChild()->getOpCodeValue() == TR::loadaddr, "The first child of abs is either the value or loadaddr");
      childNode = node->getSecondChild();
      }

   if (childNode &&
       (childNode->isNonNegative() || (node->getReferenceCount()==1)) &&
       performTransformation(comp(), "%sFolded abs for postive argument on node [%p]\n", optDetailString(), node))
      {
      TR::TreeTop::create(comp(), _curTree->getPrevTreeTop(),
                        TR::Node::create(TR::treetop, 1, childNode));
      node = replaceNode(node, childNode, _curTree);
      _alteredBlock = true;
      }

   return node;
   }

TR::Node *
J9::Simplifier::simplifyiCallMethods(TR::Node * node, TR::Block * block)
   {
   if (isRecognizedAbsMethod(node))
      {
      node = foldAbs(node);
      }
   else if (isRecognizedPowMethod(node))
      {
      static char *skipit = feGetEnv("TR_NOMATHRECOG");
      if (skipit != NULL) return node;

      int32_t numChildren = node->getNumChildren();
      // call can have 2 or 3 args.  In both cases, the last two are
      // the parameters of interest.
      TR::Node *expNode = node->getChild(numChildren-1);
      TR::Node *valueNode = node->getChild(numChildren-2);

      // In Java strictmath.pow(), if both arguments are integers,
      // then the result is exactly equal to the mathematical result
      // of raising the first argument to the power of the second argument
      // if that result can in fact be represented exactly as a double value.
      //(In the foregoing descriptions, a floating-point value is considered
      // to be an integer if and only if it is finite and a fixed point of
      // the method ceil or, equivalently, a fixed point of the method floor.
      // A value is a fixed point of a one-argument method if and only if
      // the result of applying the method to the value is equal to the value.)
      if (valueNode->getOpCodeValue() == TR::dconst &&
          expNode->getOpCodeValue() == TR::dconst &&
          valueNode->getDouble() == 10.0 && expNode->getDouble() == 4.0)
         {
         foldDoubleConstant(node, 10000.0, (TR::Simplifier *) this);
         }
      }

   return node;
   }

/**
 * Current implementation of matchAndSimplifyTimeMethods focuses on java time functions.
 */
TR::Node *
J9::Simplifier::simplifylCallMethods(TR::Node * node, TR::Block * block)
   {
   if (comp()->cg()->getSupportsCurrentTimeMaxPrecision())
      {
      TR::MethodSymbol * methodSymbol = node->getSymbol()->getMethodSymbol();
      if (methodSymbol)
         {
         if (comp()->cg()->getSupportsMaxPrecisionMilliTime() &&
             (methodSymbol->getRecognizedMethod() == TR::java_lang_System_currentTimeMillis) &&
             (methodSymbol->isJNI() || methodSymbol->isVMInternalNative() || methodSymbol->isJITInternalNative()))
            {
            node = convertCurrentTimeMillis(node, block);
            }
         else if (methodSymbol->getRecognizedMethod() == TR::java_lang_System_nanoTime &&
               (methodSymbol->isJNI() || methodSymbol->isVMInternalNative() || methodSymbol->isJITInternalNative()))
            {
            node = convertNanoTime(node, block);
            }
         }
      }
   else if (comp()->cg()->getSupportsFastCTM() && node->getNumChildren() == 0 && node->getReferenceCount() == 2)
      {
      TR::ResolvedMethodSymbol * methodSymbol = node->getSymbol()->getResolvedMethodSymbol();
      if (methodSymbol &&
          (methodSymbol->getRecognizedMethod() == TR::java_lang_System_currentTimeMillis) &&
           (methodSymbol->isVMInternalNative() || methodSymbol->isJITInternalNative()))
         {
         node = foldLongStoreOfCurrentTimeMillis(node, block);
         }
      }
   else
      {
      TR::MethodSymbol * symbol = node->getSymbol()->castToMethodSymbol();

      if (symbol &&
          (symbol->getRecognizedMethod()==TR::java_lang_Math_abs_L))
         {
         node = foldAbs(node);
         }
      }

   return node;
   }

// Convert currentTimeMillis into currentTimeMaxPrecision from:
// <tree>
//    lcall currentTimeMillis
//
//   to:
//
// <tree>
//    ldiv
//      lcall currentTimeMaxPrecision
//      lconst <divisor value>
TR::Node *J9::Simplifier::convertCurrentTimeMillis(TR::Node * node, TR::Block * block)
   {
   if (performTransformation(comp(), "%sConvert currentTimeMillis to currentTimeMaxPrecision with divide of"
            INT64_PRINTF_FORMAT " on node [%p]\n", optDetailString(), OMRPORT_TIME_HIRES_MILLITIME_DIVISOR, node))
      {
      TR::Node* lcallNode = TR::Node::createWithSymRef(node, TR::lcall, 0,
            comp()->getSymRefTab()->findOrCreateCurrentTimeMaxPrecisionSymbol());
      TR::TreeTop* callTreeTop = findTreeTop(node, block);
      TR_ASSERT(callTreeTop != NULL, "should have been able to find this tree top");

      if (node->getNumChildren() >= 1)
         {
         anchorNode(node->getFirstChild(), _curTree);
         node->getFirstChild()->recursivelyDecReferenceCount();
         }

      TR::Node* divConstNode = TR::Node::create(node, TR::lconst);
      divConstNode->setLongInt(OMRPORT_TIME_HIRES_MILLITIME_DIVISOR);

      TR::Node::recreate(node, TR::ldiv);
      node->setNumChildren(2);
      node->setAndIncChild(0, lcallNode);

      node->setAndIncChild(1, divConstNode);

      TR::Node *callTreeNode = callTreeTop->getNode();
      if (callTreeNode->getOpCode().isResolveCheck())
         {
         if (callTreeNode->getOpCodeValue() == TR::ResolveCHK)
            TR::Node::recreate(callTreeNode, TR::treetop);
         else
            TR_ASSERT(0, "Null check not expected in call to static method in class System\n");
         }

      _alteredBlock = true;
      }
   return node;
   }

// Convert java/lang/System.nanoTime() into currentTimeMaxPrecision
//
// Generate trees based on the following:
// const int64_t quotient = lcall / denominator;
// const int64_t remainder = lcall - quotient * denominator;
// const int64_t result = quotient * numerator + ((remainder * numerator) / denominator);
//
// The resulting tree:
// <tree>
//   ladd
//     lmul
//       ldiv
//         ==>lcall currentTimeMaxPrecision
//         lconst <denominator>
//       lconst <numerator>
//     ldiv
//       lmul
//         lsub
//           ==>lcall currentTimeMaxPrecision
//           lmul
//             ==>ldiv
//             ==>lconst <denominator>
//         ==>lconst <numerator>
//       ==>lconst <denominator>
TR::Node *J9::Simplifier::convertNanoTime(TR::Node * node, TR::Block * block)
   {
   if (performTransformation(comp(), "%sConvert nanoTime to currentTimeMaxPrecision with multiply of %d/%d on node [%p]\n",
            optDetailString(), OMRPORT_TIME_HIRES_NANOTIME_NUMERATOR, OMRPORT_TIME_HIRES_NANOTIME_DENOMINATOR, node))
      {
      TR::Node* lcallNode = TR::Node::createWithSymRef(node, TR::lcall, 0, comp()->getSymRefTab()->findOrCreateCurrentTimeMaxPrecisionSymbol());
      TR::TreeTop* callTreeTop = findTreeTop(node, block);
      TR_ASSERT(callTreeTop != NULL, "should have been able to find this tree top");

      if (node->getNumChildren() >= 1)
         {
         anchorNode(node->getFirstChild(), _curTree);
         node->getFirstChild()->recursivelyDecReferenceCount();
         }

      TR::Node* numeratorNode = TR::Node::lconst(node, OMRPORT_TIME_HIRES_NANOTIME_NUMERATOR);
      TR::Node* denominatorNode = TR::Node::lconst(node, OMRPORT_TIME_HIRES_NANOTIME_DENOMINATOR);

      // const int64_t quotient = lcall / denominator;
      TR::Node* quotientNode = TR::Node::create(node, TR::ldiv, 2, lcallNode, denominatorNode);

      // Calculating remainder {{
      // const int64_t remainderMulNode = quotient * denominator;
      TR::Node* remainderMulNode = TR::Node::create(node, TR::lmul, 2, quotientNode, denominatorNode);

      //const int64_t remainder = lcall - remainderMulNode;
      TR::Node* remainderNode = TR::Node::create(node, TR::lsub, 2, lcallNode, remainderMulNode);
      // }}

      // Calculating result {{
      // const int64_t addendNode1 = quotient * numerator;
      TR::Node* addendNode1 = TR::Node::create(node, TR::lmul, 2, quotientNode, numeratorNode);

      // const int64_t addend2MulNode  = (remainder * numerator);
      TR::Node* addend2MulNode = TR::Node::create(node, TR::lmul, 2, remainderNode, numeratorNode);

      // const int64_t addendNode1 = ((remainder * numerator) / denominator);
      TR::Node* addendNode2 = TR::Node::create(node, TR::ldiv, 2, addend2MulNode, denominatorNode);

      // const int64_t result = quotient * numerator + ((remainder * numerator) / denominator);
      TR::Node::recreate(node, TR::ladd);
      node->setNumChildren(2);
      node->setAndIncChild(0, addendNode1);
      node->setAndIncChild(1, addendNode2);
      // }}

      TR::Node *callTreeNode = callTreeTop->getNode();
      if (callTreeNode->getOpCode().isResolveCheck())
         {
         if (callTreeNode->getOpCodeValue() == TR::ResolveCHK)
            TR::Node::recreate(callTreeNode, TR::treetop);
         else
            TR_ASSERT(0, "Null check not expected in call to static method in class System\n");
         }

      _alteredBlock = true;
      }
   return node;
   }

TR::Node *J9::Simplifier::foldLongStoreOfCurrentTimeMillis(TR::Node * node, TR::Block * block)
   {
   // Walk the block to find this node
   //
   TR::TreeTop * tt;
   TR::TreeTop * exitTree = block->getExit();
   for (tt = block->getEntry(); tt != exitTree; tt = tt->getNextRealTreeTop())
      {
      if (((tt->getNode()->getOpCodeValue() == TR::treetop) || (tt->getNode()->getOpCodeValue() == TR::ResolveCHK)) &&
          tt->getNode()->getFirstChild() == node)
          break;
      }

   if (tt != exitTree) // found the call tree
      {
      // check if the next tree top is a [i]lstore with this value
      //
      TR::Node * nextNode = tt->getNextRealTreeTop()->getNode();
      TR::ILOpCodes opCode  = nextNode->getOpCodeValue();
      if ((opCode == TR::lstorei || opCode == TR::lstore) &&
          nextNode->getOpCode().hasSymbolReference() &&
          !nextNode->mightHaveVolatileSymbolReference())
         {
         TR::Node *valueChild;
         TR::Node * addressChild;
         if (opCode == TR::lstorei)
            {
            valueChild   = nextNode->getSecondChild();
            addressChild = nextNode->getFirstChild();
            }
         else
            {
            valueChild   = nextNode->getFirstChild();
            addressChild = 0;
            }

         if (node == valueChild &&
             performTransformation(comp(), "%sFolded long store of currentTimeMillis to use address of destination as argument on node [%p]\n", optDetailString(), node))
            {
            node->setNumChildren(1);
            TR::Node * storeAddressNode;
            if (addressChild)
               {
               if (TR::Compiler->target.is64Bit())
                  {
                  TR::Node* offsetNode = TR::Node::create(node, TR::lconst);
                  offsetNode->setLongInt((int64_t)nextNode->getSymbolReference()->getOffset());
                  storeAddressNode = TR::Node::create(TR::aladd, 2, addressChild, offsetNode);
                  }
               else
                  {
                  storeAddressNode = TR::Node::create(TR::aiadd, 2, addressChild,
                                                  TR::Node::create(node, TR::iconst, 0, (int32_t)nextNode->getSymbolReference()->getOffset()));
                  }
               }
            else
               {
               storeAddressNode = TR::Node::create(node, TR::loadaddr);
               storeAddressNode->setSymbolReference(nextNode->getSymbolReference());
               }

            node->setAndIncChild(0, storeAddressNode);
            nextNode->setNOPLongStore(true);
            //_invalidateUseDefInfo = true;
            _alteredBlock = true;
            }
         }
      }
   return node;
   }

TR::Node *
J9::Simplifier::simplifyaCallMethods(TR::Node * node, TR::Block * block)
   {
   if ((node->getOpCode().isCallDirect()) && !node->getSymbolReference()->isUnresolved() &&
       (node->getSymbol()->isResolvedMethod()) &&
       ((node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigDecimal_valueOf) ||
        (node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigDecimal_add) ||
        (node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigDecimal_subtract) ||
        (node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigDecimal_multiply) ||
        (node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigInteger_add) ||
        (node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigInteger_subtract) ||
        (node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigInteger_multiply)) &&
       (node->getReferenceCount() == 1) &&
       performTransformation(comp(), "%sRemoved dead BigDecimal/BigInteger call node [" POINTER_PRINTF_FORMAT "]\n", optDetailString(), node))
      {
      TR::Node *firstChild = node->getFirstChild();
      anchorChildren(node, _curTree);

      firstChild->incReferenceCount();

      int i;
      for (i = 0; i < node->getNumChildren(); i++)
         node->getChild(i)->recursivelyDecReferenceCount();

      TR::Node::recreate(node, TR::PassThrough);
      node->setNumChildren(1);
      }

   return node;
   }

bool
J9::Simplifier::isLegalToFold(TR::Node *node, TR::Node *firstChild)
   {
   if (!OMR::Simplifier::isLegalToFold(node, firstChild))
      return false;

   if (node->getOpCode().isConversionWithFraction() &&
       firstChild->getOpCode().isConversionWithFraction() &&
       node->getDecimalFraction() != firstChild->getDecimalFraction())
      {
      // illegal to fold a pattern like:
      // pd2d  frac=5
      //    f2pd frac=0
      //       f
      // to just f2d because the extra digits that should be introduced by the frac=5 in the parent will be lost
      return false;
      }

   if (node->getOpCode().isConversionWithFraction() &&
       !firstChild->getOpCode().isConversionWithFraction() &&
       node->getDecimalFraction() != 0)
      {
      // illegal to fold a pattern like:
      // pd2f  frac=5
      //    i2pd
      //       i
      // to just i2f because the extra digits that should be introduced by the frac=5 in the parent will be lost
      return false;
      }

   return true;
   }

TR::Node *
J9::Simplifier::getUnsafeIorByteChild(TR::Node *child, TR::ILOpCodes b2iOpCode, int32_t mulConst)
   {
   if (child->getOpCodeValue() == TR::imul &&
       child->getSecondChild()->getOpCodeValue() == TR::iconst && 
       child->getSecondChild()->getInt() == mulConst &&
       child->getFirstChild()->getOpCodeValue() == b2iOpCode && 
       child->getFirstChild()->getReferenceCount() == 1 &&
       child->getFirstChild()->getFirstChild()->getOpCodeValue() == TR::bloadi &&
       child->getFirstChild()->getFirstChild()->getReferenceCount() == 1 &&
       child->getFirstChild()->getFirstChild()->getSymbolReference() == getSymRefTab()->findOrCreateUnsafeSymbolRef(TR::Int8))
      {
      return child->getFirstChild()->getFirstChild()->getFirstChild();
      }

   return NULL;
   }

static TR::Node *getUnsafeBaseAddr(TR::Node * node, int32_t isubConst)
   {
   if (node->getOpCodeValue() == TR::isub && 
       node->getReferenceCount() == 1 &&
       node->getSecondChild()->getOpCodeValue() == TR::iconst && 
       node->getSecondChild()->getInt() == isubConst)
      {
      return node->getFirstChild();
      }

   return NULL;
   }

TR::Node *
J9::Simplifier::getLastUnsafeIorByteChild(TR::Node *child)
   {
   if (child->getOpCodeValue() == TR::bu2i && 
       child->getReferenceCount() == 1 &&
       child->getFirstChild()->getOpCodeValue() == TR::bloadi &&
       child->getFirstChild()->getReferenceCount() == 1 &&
       child->getFirstChild()->getSymbolReference() == getSymRefTab()->findOrCreateUnsafeSymbolRef(TR::Int8))
      {
      return child->getFirstChild()->getFirstChild();
      }

   return NULL;
   }

/** Check if we are reading an element from a byte array and multiplying by mulConst.

    @param mulConst will be a power of 2 because the pattern we are matching involves shifting by 8/16/24 bits.
    @return the child node (that is expected to be an aiadd or aladd) under the byte array element load.
*/
TR::Node * J9::Simplifier::getArrayByteChildWithMultiplier(TR::Node * child, TR::ILOpCodes b2iOpCode, int32_t mulConst)
   {
   // The refCount == 1 checks ensure that we only transform cases where there are no
   // other uses of the four bytes, as the performance benefit would likely be
   // negated if we have to re-read any of the individual byte values.
   if (child->getOpCodeValue() == TR::imul &&
       child->getSecondChild()->getOpCodeValue() == TR::iconst && child->getSecondChild()->getInt() == mulConst &&
       child->getFirstChild()->getOpCodeValue() == b2iOpCode && child->getFirstChild()->getReferenceCount() == 1 &&
       child->getFirstChild()->getFirstChild()->getOpCodeValue() == TR::bloadi &&
       child->getFirstChild()->getFirstChild()->getReferenceCount() == 1 &&
       child->getFirstChild()->getFirstChild()->getSymbolReference()->getSymbol()->isArrayShadowSymbol())
      {
      return child->getFirstChild()->getFirstChild()->getFirstChild();
      }
   return NULL;
   }

/** Check if we are simply reading an element from a byte array.

 This "last" byte in the pattern we are matching does not involve any shifting and we will read and OR 
 this byte into the 32-bit word where the other three bytes we read have been shifted by 8/16/24 bits.

 @return the child node (that is expected to be an aiadd or aladd) under the byte array element load. 
*/
TR::Node * J9::Simplifier::getLastArrayByteChild(TR::Node * child)
   {
   // The refCount == 1 checks ensure that we only transform cases where there are no
   // other uses of the four bytes, as the performance benefit would likely be
   // negated if we have to re-read any of the individual byte values.
   if (child->getOpCodeValue() == TR::bu2i && child->getReferenceCount() == 1 &&
       child->getFirstChild()->getOpCodeValue() == TR::bloadi &&
       child->getFirstChild()->getReferenceCount() == 1 &&
       child->getFirstChild()->getSymbolReference()->getSymbol()->isArrayShadowSymbol())
      {
      return child->getFirstChild()->getFirstChild();
      }
   return NULL;
   }

/** Get the first child of an aiadd/aladd which will be a node representing the underlying array.

  @return the first child or 0 if we do not pass in an aiadd/aladd node since that would break the pattern we are matching.
*/
TR::Node * J9::Simplifier::getArrayBaseAddr(TR::Node * node)
   {
   if (node->getOpCode().isArrayRef() && node->getReferenceCount() == 1)
      {
      return node->getFirstChild();
      }
   return NULL;
   }

/** Check if we are reading a byte array element that is offset by a particular constant amount 
off a base index value.

Since the pattern we are matching involves reading four successive byte array elements, we are 
essentially looking for whatever base index value the first array element access used being offset
by +1/+2/+3 for the subsequent array accesses.

@return 0 if the constant offset value passed in did not match what is expected at the given byte 
array load, otherwise return the base index value, e.g. array accesses a[i], a[i + 1], a[i + 2], 
a[i + 3] will return i as the base index value. 
*/
TR::Node * J9::Simplifier::getArrayOffset(TR::Node * node, int32_t isubConst)
   {
   // The refCount == 1 checks ensure that we only transform cases where there are no
   // other uses of the node, as the performance benefit would likely be
   // negated if we have to re-generate the node after transforming the sequence to
   // an ibyteswap.
   if (node->getOpCode().isArrayRef() && node->getReferenceCount() == 1 &&
       node->getSecondChild()->getOpCode().isSub() && 
       node->getSecondChild()->getReferenceCount() == 1 &&
       ((node->getSecondChild()->getSecondChild()->getOpCodeValue() == TR::iconst && node->getSecondChild()->getSecondChild()->getInt() == isubConst) ||
        (node->getSecondChild()->getSecondChild()->getOpCodeValue() == TR::lconst && node->getSecondChild()->getSecondChild()->getLongInt() == (int64_t)isubConst))
      )
      {
      return node->getSecondChild()->getFirstChild();
      }
   return NULL;
   }

TR::Node *
J9::Simplifier::simplifyiOrPatterns(TR::Node *node)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR::ILOpCodes firstChildOp  = firstChild->getOpCodeValue();
   TR::ILOpCodes secondChildOp = secondChild->getOpCodeValue();

   // optimize java/nio/DirectByteBuffer.getInt(I)I
   //
   // Little Endian
   // ior
   //   ior
   //     imul
   //       bu2i
   //         ibload #245[0x107AF564] Shadow[<Unsafe shadow sym>]
   //           isub
   //             l2i
   //               lload #202[0x107A1050] Parm[<parm 1 J>]
   //             iconst -1
   //       iconst 256
   //     ior
   //       imul
   //         bu2i
   //           ibload #245[0x107AF564] Shadow[<Unsafe shadow sym>]
   //             isub
   //               ==>l2i at [0x107AE8F4]
   //               iconst -3
   //         iconst 16777216
   //       imul
   //         bu2i
   //           ibload #245[0x107AF564] Shadow[<Unsafe shadow sym>]
   //             isub
   //               ==>l2i at [0x107AE8F4]
   //               iconst -2
   //         iconst 65536
   //   bu2i
   //     ibload #245[0x107AF564] Shadow[<Unsafe shadow sym>]
   //       ==>l2i at [0x107AE8F4]
   //

   TR::Node *byte1, *byte2, *byte3, *byte4, *temp, *addr;
   if (firstChild->getReferenceCount() == 1 &&
       firstChildOp == TR::ior &&
       firstChild->getSecondChild()->getOpCodeValue() == TR::ior && 
       (byte1 = getUnsafeIorByteChild(firstChild->getSecondChild()->getFirstChild(), TR::bu2i, 16777216)) &&
       (byte2 = getUnsafeIorByteChild(firstChild->getSecondChild()->getSecondChild(), TR::bu2i, 65536)) &&
       (byte3 = getUnsafeIorByteChild(firstChild->getFirstChild(), TR::bu2i, 256)) &&
       (byte4 = getLastUnsafeIorByteChild(node->getSecondChild())))
      {
      if (TR::Compiler->target.cpu.isLittleEndian())
         {
         temp = byte1, byte1 = byte4, byte4 = temp;
         temp = byte2, byte2 = byte3, byte3 = temp;
         }

      if ((addr = getUnsafeBaseAddr(byte2, -1)) && addr == byte1 &&
          (addr = getUnsafeBaseAddr(byte3, -2)) && addr == byte1 &&
          (addr = getUnsafeBaseAddr(byte4, -3)) && addr == byte1 &&
          performTransformation(comp(), "%sconvert ior to iiload node [" POINTER_PRINTF_FORMAT "]\n", optDetailString(), node))
         {
         TR::Node::recreate(node, TR::iloadi);
         node->setNumChildren(1);
         node->setSymbolReference(getSymRefTab()->findOrCreateUnsafeSymbolRef(TR::Int32));
         node->setAndIncChild(0, byte1);

         // now remove the two children
         firstChild->recursivelyDecReferenceCount();
         secondChild->recursivelyDecReferenceCount();

         return node;
         }
      }

   static char *disableIORByteSwap = feGetEnv("TR_DisableIORByteSwap");

   firstChild = node->getFirstChild();
   secondChild = node->getSecondChild();
   if (!disableIORByteSwap &&
       !TR::Compiler->target.cpu.isBigEndian() &&  // the bigEndian case needs more thought
       secondChild->getOpCodeValue() == TR::ior)
      {
      /*
         A common application pattern for reading from a byte array and performing endianness conversion
         will perform four consecutive reads from a byte array, multiplying three of the bytes 
         by 256, 65536 and 16777216 and or'ing the results together. We can detect this pattern and
         transform it to a single ibyteswap. Note that the ishl in the trees below have been converted
         to imul by the time we reach here.

               ior
                  ishl
                     b2i
                        bloadi  <array-shadow>
                           aladd
                              ==>aLoad
                              lsub
                                 i2l
                                    ==>iLoad
                                 lconst -11
                     iconst 24
                  ior
                     ishl
                        bu2i
                           bloadi  <array-shadow>
                              aladd
                                 ==>aLoad
                                 lsub
                                    ==>i2l
                                    lconst -10
                         iconst 16
                     ior
                        ishl
                           bu2i
                              bloadi  <array-shadow>
                                 aladd
                                    ==>aLoad
                                    lsub
                                       ==>i2l
                                       lconst -9
                           iconst 8
                        bu2i
                           bloadi  <array-shadow>
                              aladd
                                 ==>aLoad
                                lsub
                                   ==>i2l
                                   lconst -8
              */
      TR::Node * byte1, * byte2, * byte3, * byte4, * temp, * addr, *offset, *addr1, *offset1;
      int32_t initialConstantOffset;

      const int SHIFT8MUL=256;
      const int SHIFT16MUL=65536;
      const int SHIFT24MUL=16777216;

      // Check for the expected pattern of four array accesses
      if (secondChild->getSecondChild()->getOpCodeValue() == TR::ior && secondChild->getReferenceCount() == 1 &&
          (byte1 = getArrayByteChildWithMultiplier(firstChild, TR::b2i, SHIFT24MUL)) &&
          (byte2 = getArrayByteChildWithMultiplier(secondChild->getFirstChild(), TR::bu2i, SHIFT16MUL)) &&
          (byte3 = getArrayByteChildWithMultiplier(secondChild->getSecondChild()->getFirstChild(), TR::bu2i, SHIFT8MUL)) &&
          (byte4 = getLastArrayByteChild(secondChild->getSecondChild()->getSecondChild())))
         {

         // This caused incorrect behaviour, so I've disabled the entire code path for bigEndian (see above)
         //if (TR::Compiler->target.cpu.isBigEndian())
         //   {
         //   temp = byte1, byte1 = byte4, byte4 = temp;
         //   temp = byte2, byte2 = byte3, byte3 = temp;
         //   }

         // Get the address and offset from the first array access
         // These will be compared with the subsequent array accesses to confirm that
         // the code is reading four contiguous locations.
         addr1 = NULL;
         if (byte1->getOpCode().isArrayRef() && byte1->getReferenceCount() == 1)
            addr1 = byte1->getFirstChild();

         offset1 = NULL;
         if (addr1 && byte1->getSecondChild()->getOpCode().isSub() && byte1->getSecondChild()->getReferenceCount() == 1)
            offset1 = byte1->getSecondChild()->getFirstChild();

         // Get the array offset from the first array access
         bool expectedConstantOffset1 = false;
         if (offset1)
            {
            if ((byte1->getSecondChild()->getSecondChild()->getOpCodeValue() == TR::lconst &&
               byte1->getSecondChild()->getSecondChild()->getLongInt() <= ((int64_t) TR::getMaxSigned<TR::Int32>()) &&
                 byte1->getSecondChild()->getSecondChild()->getLongInt() >= ((int64_t) TR::getMinSigned<TR::Int32>())))
               {
               expectedConstantOffset1 = true;
               initialConstantOffset = (int32_t) byte1->getSecondChild()->getSecondChild()->getLongInt();
               }
            else if (byte1->getSecondChild()->getSecondChild()->getOpCodeValue() == TR::iconst)
               {
               expectedConstantOffset1 = true;
               initialConstantOffset = byte1->getSecondChild()->getSecondChild()->getInt();
               }
            }

         // Confirm that all four array accesses deal with the same base address and offset, i.e. the code
         // is reading four contiguous bytes.
         if (expectedConstantOffset1 &&
             (getArrayBaseAddr(byte2) == addr1) && (getArrayOffset(byte2, initialConstantOffset+1) == offset1) &&
             (getArrayBaseAddr(byte3) == addr1) && (getArrayOffset(byte3, initialConstantOffset+2) == offset1) &&
             (getArrayBaseAddr(byte4) == addr1) && (getArrayOffset(byte4, initialConstantOffset+3) == offset1) &&
              performTransformation(comp(), "%sconvert ior to ibyteswap node [" POINTER_PRINTF_FORMAT "]\n", optDetailString(), node))
            {
            TR::Node::recreateWithSymRef(node, TR::iloadi, getSymRefTab()->findOrCreateUnsafeSymbolRef(TR::Int32));
            node->setNumChildren(1);
            node->setAndIncChild(0, byte4);

            // now remove the two children
            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();

            return node;
            }
         }
      }

   // recognize Long.signum(), which maps to TR::lcmp
   //   ior
   //     l2i                    <- fc
   //      lshr                  <- fgc
   //        load X              <- loadVal
   //        iconst 63
   //     l2i                    <- sc
   //      lushr                 <- sgc
   //        lneg
   //          load X
   //        iconst 63
   //
   //
   //
   //
   if (firstChild->getOpCodeValue() == TR::l2i &&
      secondChild->getOpCodeValue() == TR::l2i &&
      firstChild ->getFirstChild()->getOpCodeValue() == TR::lshr &&
      secondChild->getFirstChild()->getOpCodeValue() == TR::lushr)
      {
      TR::Node *firstGrandChild = firstChild->getFirstChild();
      TR::Node *secondGrandChild = secondChild->getFirstChild();
      if (secondGrandChild->getFirstChild()->getOpCodeValue() == TR::lneg &&
         firstGrandChild->getSecondChild()->getOpCodeValue() == TR::iconst &&
         firstGrandChild->getSecondChild()->getInt() == 63 &&
         secondGrandChild->getSecondChild()->getOpCodeValue() == TR::iconst &&
         secondGrandChild->getSecondChild()->getInt() == 63)
         {
         TR::Node *loadVal = firstGrandChild->getFirstChild();
         if (secondGrandChild->getFirstChild()->getFirstChild() == loadVal &&
            (loadVal->getOpCode().isLoadVar() || loadVal->getOpCode().isLoadReg()) &&
            performTransformation(comp(), "%sTransform ior to lcmp [" POINTER_PRINTF_FORMAT "]\n", optDetailString(), node))
            {
            TR::Node::recreate(node, TR::lcmp);
            TR::Node * constZero = TR::Node::create(secondChild, TR::lconst, 0);
            constZero->setLongInt(0);
            node->setFirst(replaceNode(firstChild,loadVal, _curTree));
            node->setSecond(replaceNode(secondChild, constZero, _curTree));
            return node;
            }
         }
      }

   return NULL;
   }

TR::Node *
J9::Simplifier::getOrOfTwoConsecutiveBytes(TR::Node * ior)
   {
   // Big Endian
   // ior
   //   imul
   //     b2i
   //       ibload #231[0x10D06670] Shadow[unknown field]
   //         address
   //     iconst 256
   //   bu2i
   //     ibload #231[0x10D06670] Shadow[unknown field]
   //       isub
   //         ==>address
   //         iconst -1
   //
   // Little Endian
   // ior
   //   imul
   //     b2i
   //       ibload #231[0x10D06670] Shadow[unknown field]
   //         isub
   //           address
   //           iconst -1
   //     iconst 256
   //   bu2i
   //     ibload #231[0x10D06670] Shadow[unknown field]
   //       ==>address
   //
   TR::Node *byte1, *byte2, *temp, *addr;
   if ((byte1 = getUnsafeIorByteChild(ior->getFirstChild(), TR::b2i, 256)) &&
       (byte2 = getLastUnsafeIorByteChild(ior->getSecondChild())))
      {
      if (TR::Compiler->target.cpu.isLittleEndian())
         temp = byte1, byte1 = byte2, byte2 = temp;

      if ((addr = getUnsafeBaseAddr(byte2, -1)) && addr == byte1)
         {
         byte1->decReferenceCount();
         return byte1;
         }
      }

   return NULL;
   }

TR::Node *
J9::Simplifier::simplifyi2sPatterns(TR::Node *node)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Node *address;

   if (firstChild->getOpCodeValue() == TR::ior && 
       firstChild->getReferenceCount() == 1 &&
       (address = getOrOfTwoConsecutiveBytes(firstChild)) &&
       performTransformation(comp(), "%sconvert ior to isload node [" POINTER_PRINTF_FORMAT "]\n", optDetailString(), node))
      {
      TR::Node::recreate(node, TR::sloadi);
      node->setSymbolReference(getSymRefTab()->findOrCreateUnsafeSymbolRef(TR::Int16));
      node->setChild(0, address);
      return node;
      }

   return NULL;
   }

/**
 * see if node is a dcall to java.lang.Math.sqrt (or strict equivalent)  *and*
 * all children are f2d then create a call to a single precision call
 * the call will look like
 *
 *     treetop
 *        dcall Method[sqrt]
 *          f2d
 *            fload #111
 *     fstore
 *        d2f
 *          ==> above dcall
 *
 * the treetop is put there in case the call has a side effect.
 * we know it doesn't, so replace the call.  However, because
 * we don't see all the references, we only do this opt if
 * reference count is exactly two.  If we miss more opportunities
 * we can always do a special pass inspecting all calls to see
 * if they feed d2fs.
 */

TR::Node *
J9::Simplifier::simplifyd2fPatterns(TR::Node *node)
   {
   TR::Node *sqrtCall = node->getFirstChild();

   if (sqrtCall->getReferenceCount() != 2)
      return NULL;

   if (!comp()->cg()->supportsSinglePrecisionSQRT()) 
      return NULL;

   if (sqrtCall->getOpCodeValue() != TR::dcall) 
      return NULL;

   static char *skipit = feGetEnv("TR_NOFSQRT");
   if (skipit) return NULL;

   TR::MethodSymbol *symbol = sqrtCall->getSymbol()->castToMethodSymbol();
   TR::MethodSymbol *methodSymbol = sqrtCall->getSymbol()->getMethodSymbol();
   if (!methodSymbol ||
       (methodSymbol->getRecognizedMethod() != TR::java_lang_Math_sqrt &&
        methodSymbol->getRecognizedMethod() != TR::java_lang_StrictMath_sqrt))
      {
      return NULL;
      }

   // now check the child
   TR::Node *f2dChild;
   int32_t numKids = sqrtCall->getNumChildren();
   if (2 == numKids)
     f2dChild= sqrtCall->getSecondChild();// on ia32, actually two children
   else
     f2dChild= sqrtCall->getFirstChild();

   if (f2dChild->getOpCodeValue() != TR::f2d) 
      return NULL; // TODO: could get more aggressive

   if (!performTransformation(comp(), "%sTransforming [" POINTER_PRINTF_FORMAT "] (double)sqrt(f2d(x))->(float)sqrt(x)\n", optDetailString(), sqrtCall)) 
      {
      return NULL;
      }

   TR::SymbolReference *fsqrtSymRef = comp()->getSymRefTab()->findOrCreateSinglePrecisionSQRTSymbol();
   TR::TreeTop* callTreeTop = findTreeTop(sqrtCall, _curTree->getEnclosingBlock()->startOfExtendedBlock());
   TR_ASSERT(callTreeTop != NULL, "should have been able to find this tree top (call %p) in block %d", sqrtCall, _curTree->getEnclosingBlock()->getNumber());

   TR::Node::recreate(sqrtCall, TR::fcall);
   sqrtCall->setSymbolReference(fsqrtSymRef);

   // replace child
   TR::Node *newf2dChild = replaceNode(f2dChild, f2dChild->getFirstChild(), _curTree);
   sqrtCall->setChild(numKids-1, newf2dChild);

   TR::Node *callTreeNode = callTreeTop->getNode();
   if (callTreeNode->getOpCode().isResolveCheck())
      {
      if (callTreeNode->getOpCodeValue() == TR::ResolveCHK)
         TR::Node::recreate(callTreeNode, TR::treetop);
      else
         TR_ASSERT(0, "Null check not expected in call to static method sqrt\n");
      }

   return sqrtCall;
   }

bool
J9::Simplifier::isBoundDefinitelyGELength(TR::Node *boundChild, TR::Node *lengthChild)
   {
   if (OMR::Simplifier::isBoundDefinitelyGELength(boundChild, lengthChild))
      return true;

   if (boundChild->getOpCode().isArrayLength())
      {
      TR::Node *first = boundChild->getFirstChild();

      if (first->getOpCodeValue() == TR::aloadi       &&
          lengthChild->getOpCodeValue() == TR::iloadi &&
          first->getFirstChild() == lengthChild->getFirstChild())
         {
         TR::SymbolReference * boundSymRef  = first->getSymbolReference();
         TR::SymbolReference * lengthSymRef = lengthChild->getSymbolReference();
         if ((boundSymRef->getSymbol()->getRecognizedField()  == TR::Symbol::Java_lang_String_value &&
              lengthSymRef->getSymbol()->getRecognizedField() == TR::Symbol::Java_lang_String_count) ||
             (boundSymRef->getSymbol()->getRecognizedField()  == TR::Symbol::Java_lang_StringBuffer_value &&
              lengthSymRef->getSymbol()->getRecognizedField() == TR::Symbol::Java_lang_StringBuffer_count) ||
             (boundSymRef->getSymbol()->getRecognizedField()  == TR::Symbol::Java_lang_StringBuilder_value &&
              lengthSymRef->getSymbol()->getRecognizedField() == TR::Symbol::Java_lang_StringBuilder_count))
            {
            return true;
            }
         }
      }

   return false;
   }

static bool symRefPairMatches(TR::SymbolReference *actual1, TR::SymbolReference *actual2, TR::SymbolReference *desired1, TR::SymbolReference *desired2)
   {
   if (actual1 && actual2 && desired1 && desired2)
      {
      if (actual1 == desired1 && actual2 == desired2)
         return true;
      if (actual1 == desired2 && actual2 == desired1)
         return true;

      TR::Symbol *actual1Symbol = actual1->getSymbol();
      TR::Symbol *actual2Symbol = actual2->getSymbol();
      TR::Symbol *desired1Symbol = desired1->getSymbol();
      TR::Symbol *desired2Symbol = desired2->getSymbol();

      if (actual1Symbol == desired1Symbol && actual2Symbol == desired2Symbol)
         return true;
      if (actual1Symbol == desired2Symbol && actual2Symbol == desired1Symbol)
         return true;
      }
   return false;
   }

TR::Node *
J9::Simplifier::simplifyIndirectLoadPatterns(TR::Node *node)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::ILOpCodes nodeOp = node->getOpCodeValue();
   TR::ILOpCodes childOp = firstChild->getOpCodeValue();
   TR::SymbolReference *nodeSymref  = node->getSymbolReference();
   TR_ASSERT(nodeSymref, "Unexpected null symbol reference on load node\n");

   if (nodeOp == TR::aloadi || nodeOp == TR::iloadi || nodeOp == TR::lloadi)
      {
      if (childOp == TR::aloadi || childOp == TR::iloadi || childOp == TR::lloadi)
         {
         // Complementary reference fields, where a->b->c == a
         //
         bool fieldsAreComplementary = false;

         TR::SymbolReference *childSymref = firstChild->getSymbolReference();

         if (symRefPairMatches(nodeSymref, childSymref, getSymRefTab()->findClassFromJavaLangClassSymbolRef(), getSymRefTab()->findJavaLangClassFromClassSymbolRef()))
            fieldsAreComplementary = true;

         if (symRefPairMatches(nodeSymref, childSymref, getSymRefTab()->findClassFromJavaLangClassAsPrimitiveSymbolRef(), getSymRefTab()->findJavaLangClassFromClassSymbolRef()))
            fieldsAreComplementary = true;

         TR::Node *grandchild = firstChild->getFirstChild();
         if (fieldsAreComplementary && 
             performTransformation(comp(), "%sFolded complementary field load [%p]->%s->%s\n",
               optDetailString(), grandchild,
               nodeSymref ->getName(getDebug()),
               childSymref->getName(getDebug())))
            {
            TR::Node *replacement = grandchild;
            if (replacement->getDataType() != node->getDataType())
               {
               TR::ILOpCodes convOpCode = TR::ILOpCode::getProperConversion(replacement->getDataType(), node->getDataType(), false);
               TR_ASSERT(convOpCode != TR::BadILOp, "Conversion between two different data types requires an opcode");
               replacement = TR::Node::create(convOpCode, 1, grandchild);
               // Note: be wary of collected refs.  We don't want to convert those accidentally to primitives.
               }
            return replaceNode(node, replacement, _curTree);
            }
         }
      }

   return NULL;
   }

void
J9::Simplifier::setNodePrecisionIfNeeded(TR::Node *baseNode, TR::Node *node, TR::NodeChecklist& visited)
   {
   if (visited.contains(node))
      return;

   visited.add(node);
   if (baseNode->getType().isAnyPacked())
      {
      node->setPDMulPrecision();
      }

   for (int i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node *child = node->getChild(i);
      setNodePrecisionIfNeeded(baseNode, child, visited);
      }

   return;
   }
