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

#include "env/VMJ9.h"
#include "codegen/InstOpCode.hpp"
#include "codegen/CodeGenerator.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "optimizer/LocalCSE.hpp"
#include "optimizer/Optimization_inlines.hpp"

bool
J9::LocalCSE::shouldTransformBlock(TR::Block *block)
   {
   if (!OMR::LocalCSE::shouldTransformBlock(block))
      return false;

   if (self()->comp()->getMethodHotness() < warm &&
       block->getFrequency() < TR::Options::_localCSEFrequencyThreshold &&
       !self()->comp()->compileRelocatableCode())
      return false;

   return true;
   }

bool
J9::LocalCSE::shouldCommonNode(TR::Node *parent, TR::Node *node)
   {
   if (!OMR::LocalCSE::shouldCommonNode(parent, node))
      return false;

   if (parent != NULL)
      {
      // Commoning NULL pointers under a guard might result in dereferencing a NULL pointer
      if (parent->isNopableInlineGuard() &&  node->getOpCode().hasSymbolReference())
         {
         TR::Symbol* symbol = node->getSymbolReference()->getSymbol();

         if (symbol->isStatic() && symbol->getStaticSymbol()->getStaticAddress() == NULL)
            {
            return false;
            }
         }

      // Prevent commoning of the first child of BCDCHK if the recognized DAA API checkOverflow argument is true.
      //
      // The BCDCHK node is well structured. Its first child represents a BCD operation that may raise a hardware
      // interrupt if malformed input is provided. The evaluation of BCDCHK creates an OOL path to which we branch to
      // from the signal handler if a hardware interrupt was caught. This OOL path simply reconstructs the original
      // Java call to the DAA API and delegates the respective computation.
      //
      // Some DAA APIs have a conditional parameter called checkOverflow which lets the user control whether the
      // respective API will raise a Java exception if overflow is detected. Thus if the checkOverflow argument is
      // true then the evaluation of BCDCHK may or may not raise a Java exception. The issue with this is the following
      // example scenario:
      //
      // n1n   BCDCHK
      // n2n     pdshl
      //           ...
      //         ...
      // n3n     iconst 0
      // ...
      // n4n   BCDCHK
      // n5n     pdshl
      //           ...
      //         ...
      // n6n     iconst 1
      //
      // Assuming n2n and n5n represent the same operation local CSE will attempt to common n2n with n5n. However given
      // the above this can result in unwanted behavior. Commoning n5n with n2n is incorrect because the BCDCHK at n4n
      // had checkOverflow argument as true (n6n) and thus the original DAA Java calls were in fact different. Assuming
      // the BCDCHKs at n1n and n4n both raised hardware interrupts the fallback OOL path in case of n1n would have not
      // raised a Java exception and the execution would resume as normal. However in case on n6n the fallback OOL path
      // may or may not raise a Java exception so execution could in fact diverge to a different path. Hence it is not
      // correct to common n5n and n2n in such cases.
      //
      // The following block of code checks for the above case and prevents commoning of n5n with n2n.
      //
      // NOTE: if n4n came before n1n in the treetop order then the commoning from n5n to n2n would be in fact valid,
      // and the current implementation will allow it. However if between two BCDCHK treetops which both do overflow
      // checking (i.e. two distinct instances of n4n) the current implementation will not allow the commoning to
      // happen even though it is valid. Enabling the commoning is such cases is a lot more complicated from an
      // implementation perspective and is likely not worth the return on investment.
      if (parent->getOpCodeValue() == TR::BCDCHK && parent->getFirstChild() == node)
         {
         TR::MethodSymbol* bcdchkMethodSymbol = parent->getSymbolReference()->getSymbol()->getMethodSymbol();

         TR_ASSERT(bcdchkMethodSymbol != NULL, "BCDCHK should always have a resolved method symbol since it was reduced in DataAccessAccelerator optimization");

         switch(bcdchkMethodSymbol->getRecognizedMethod())
            {
            case TR::com_ibm_dataaccess_PackedDecimal_lessThanPackedDecimal_:
            case TR::com_ibm_dataaccess_PackedDecimal_lessThanOrEqualsPackedDecimal_:
            case TR::com_ibm_dataaccess_PackedDecimal_greaterThanPackedDecimal_:
            case TR::com_ibm_dataaccess_PackedDecimal_greaterThanOrEqualsPackedDecimal_:
            case TR::com_ibm_dataaccess_PackedDecimal_equalsPackedDecimal_:
            case TR::com_ibm_dataaccess_PackedDecimal_notEqualsPackedDecimal_:
            case TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal_:
            case TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToPackedDecimal_:
            case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToExternalDecimal_:
            case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToUnicodeDecimal_:
            case TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToPackedDecimal_:
               {
               // No optional checkOverflow parameter on these APIs
               break;
               }

            case TR::com_ibm_dataaccess_PackedDecimal_addPackedDecimal_:
            case TR::com_ibm_dataaccess_PackedDecimal_subtractPackedDecimal_:
            case TR::com_ibm_dataaccess_PackedDecimal_multiplyPackedDecimal_:
            case TR::com_ibm_dataaccess_PackedDecimal_dividePackedDecimal_:
            case TR::com_ibm_dataaccess_PackedDecimal_remainderPackedDecimal_:
            case TR::com_ibm_dataaccess_PackedDecimal_shiftLeftPackedDecimal_:
            case TR::com_ibm_dataaccess_PackedDecimal_shiftRightPackedDecimal_:
            case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger_:
            case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger_ByteBuffer_:
            case TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal_:
            case TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal_ByteBuffer_:
            case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong_:
            case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong_ByteBuffer_:
            case TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal_:
            case TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal_ByteBuffer_:
               {
               TR::Node* checkOverflowNode = parent->getLastChild();

               if (!(checkOverflowNode->getOpCode().isLoadConst() && checkOverflowNode->getConstValue() == 0))
                  {
                  traceMsg(comp(), "Skipping propagation of %s [%p] into the first child of %s [%p] because of potential overflow checking\n", node->getOpCode().getName(), node, parent->getOpCode().getName(), parent);

                  return false;
                  }

               break;
               }

            default:
               {
               TR_ASSERT_FATAL(false, "Unrecognized DAA method symbol in BCDCHK [%p]\n", parent);

               return false;
               }
            }
         }
      }

   return true;
   }

bool
J9::LocalCSE::shouldCopyPropagateNode(TR::Node *parent, TR::Node *node, int32_t childNum, TR::Node *storeNode)
   {
   if (!OMR::LocalCSE::shouldCopyPropagateNode(parent, node, childNum, storeNode))
      return false;

   // External float types come in as aggregates and an effort during ilgen (extFloatFixup) is made to fix
   // up all the aggr types to extFloat.
   // However due to late arrayOp scalarization there is a chance that CSE may create incorrect types during propagation
   // so prevent these transformations from happening.
   // The cost of adding a conversion to make things type correct is too high here (a runtime call) so disallow these.

   int32_t childAdjust = storeNode->getOpCode().isWrtBar() ? 2 : 1;
   int32_t maxChild = storeNode->getNumChildren() - childAdjust;
   TR::Node *rhsOfStoreDefNode = storeNode->getChild(maxChild);
   bool propagationIsTypeCorrect = true;

   if (parent && parent->getChild(childNum))
      {
      TR::Node *oldNode = parent->getChild(childNum);
      TR::DataType oldType = oldNode->getType();
      TR::DataType newType = rhsOfStoreDefNode->getType();
      if (oldType.isBCD() != newType.isBCD() ||
          oldType.isFloatingPoint() != newType.isFloatingPoint())
         propagationIsTypeCorrect = false;

       if (!propagationIsTypeCorrect && (comp()->cg()->traceBCDCodeGen() || trace()))
         {
         int32_t lineNumber = comp()->getLineNumber(rhsOfStoreDefNode);
         traceMsg(comp(),"z^z : skipping type invalid propagation : parent %s (%p), rhsOfStoreDefNode %s (%p) line_no=%d (offset %06X)\n",
                 parent->getOpCode().getName(),parent,rhsOfStoreDefNode->getOpCode().getName(),rhsOfStoreDefNode,lineNumber,lineNumber);
         }

      if (!propagationIsTypeCorrect)
         return false;
      }

   return true;
   }
