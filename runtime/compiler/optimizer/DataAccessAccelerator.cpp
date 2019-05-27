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

#include "optimizer/DataAccessAccelerator.hpp"

#include <algorithm>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/RegisterConstants.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/CompilerEnv.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/NodePool.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ParameterSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "infra/Stack.hpp"
#include "infra/TRCfgEdge.hpp"
#include "infra/TRCfgNode.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/OSRGuardRemoval.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/TransformUtil.hpp"
#include "ras/Debug.hpp"

#define IS_VARIABLE_PD2I(callNode) (!isChildConst(callNode, 2) || !isChildConst(callNode, 3))

TR_DataAccessAccelerator::TR_DataAccessAccelerator(TR::OptimizationManager* manager)
   :
      TR::Optimization(manager)
   {
   // Void
   }

int32_t TR_DataAccessAccelerator::perform()
   {
   int32_t result = 0;

   if (!comp()->getOption(TR_DisableIntrinsics) &&
       !comp()->getOption(TR_MimicInterpreterFrameShape) &&

       // We cannot handle arraylets because hardware intrinsics act on contiguous memory
       !comp()->generateArraylets()&& !TR::Compiler->om.useHybridArraylets())
     {

     // A vector to keep track of variable packed decimal calls
     TR::StackMemoryRegion stackMemoryRegion(*(comp()->trMemory()));
     TreeTopContainer variableCallTreeTops(stackMemoryRegion);

     for (TR::AllBlockIterator iter(optimizer()->getMethodSymbol()->getFlowGraph(), comp());
          iter.currentBlock() != NULL;
          ++iter)
        {
        TR::Block* block = iter.currentBlock();

        result |= performOnBlock(block, &variableCallTreeTops);
        }

     result |= processVariableCalls(&variableCallTreeTops);
     }

   if (result != 0)
      {
      optimizer()->setUseDefInfo(NULL);
      optimizer()->setValueNumberInfo(NULL);
      optimizer()->setAliasSetsAreValid(false);
      }

   return result;
   }

int32_t
TR_DataAccessAccelerator::processVariableCalls(TreeTopContainer* variableCallTreeTops)
   {
   int result = 0;

   // Process variable precision calls after iterating through all the nodes
   for(int i = 0; i < variableCallTreeTops->size(); ++i)
      {
      TR::TreeTop* treeTop = variableCallTreeTops->at(i);
      TR::Node* callNode = treeTop->getNode()->getChild(0);
      TR::ResolvedMethodSymbol* callSymbol = callNode->getSymbol()->getResolvedMethodSymbol();
      if (callSymbol != NULL)
         {
         if (!comp()->getOption(TR_DisablePackedDecimalIntrinsics))
            {
            switch (callSymbol->getRecognizedMethod())
               {
               // DAA Packed Decimal <-> Integer
               case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger_:
                  {
                  result |= generatePD2IVariableParameter(treeTop, callNode, true, false); continue;
                  }
               case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger_ByteBuffer_:
                  {
                  result |= generatePD2IVariableParameter(treeTop, callNode, true, true); continue;
                  }

                  // DAA Packed Decimal <-> Long
               case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong_:
                  {
                  result |= generatePD2IVariableParameter(treeTop, callNode, false, false); continue;
                  }
               case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong_ByteBuffer_:
                  {
                  result |= generatePD2IVariableParameter(treeTop, callNode, false, true); continue;
                  }
               default:
                  break;
               }
            }
         }
      }

   return result;
   }

const char *
TR_DataAccessAccelerator::optDetailString() const throw()
   {
   return "O^O DATA ACCESS ACCELERATOR: ";
   }

int32_t TR_DataAccessAccelerator::performOnBlock(TR::Block* block, TreeTopContainer* variableCallTreeTops)
   {
   int32_t blockResult = 0;
   bool requestOSRGuardRemoval = false;

   for (TR::TreeTopIterator iter(block->getEntry(), comp()); iter != block->getExit(); ++iter)
      {
      TR::Node* currentNode = iter.currentNode();
      if (currentNode->getOpCodeValue() == TR::treetop)
         {
         currentNode = currentNode->getChild(0);
         }

      if (currentNode != NULL && currentNode->getOpCode().isCall())
         {
         int32_t result = 0;
         bool matched = false;

         TR::TreeTop* treeTop = iter.currentTree();

         TR::Node* callNode = currentNode;

         TR::Node* returnNode = NULL;

         TR::ResolvedMethodSymbol* callSymbol = callNode->getSymbol()->getResolvedMethodSymbol();

         if (callSymbol != NULL)
            {
            if (!comp()->getOption(TR_DisableMarshallingIntrinsics))
               {
               switch (callSymbol->getRecognizedMethod())
                  {
               // ByteArray Marshalling methods
               case TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShort_:
                  returnNode = insertIntegerSetIntrinsic(treeTop, callNode, 2, 2);
                  break;
               case TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShortLength_:
                  returnNode = insertIntegerSetIntrinsic(treeTop, callNode, 2, 0);
                  break;
               case TR::com_ibm_dataaccess_ByteArrayMarshaller_writeInt_:
                  returnNode = insertIntegerSetIntrinsic(treeTop, callNode, 4, 4);
                  break;
               case TR::com_ibm_dataaccess_ByteArrayMarshaller_writeIntLength_:
                  returnNode = insertIntegerSetIntrinsic(treeTop, callNode, 4, 0);
                  break;
               case TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLong_:
                  returnNode = insertIntegerSetIntrinsic(treeTop, callNode, 8, 8);
                  break;
               case TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLongLength_:
                  returnNode = insertIntegerSetIntrinsic(treeTop, callNode, 8, 0);
                  break;

               case TR::com_ibm_dataaccess_ByteArrayMarshaller_writeFloat_:
                  returnNode = insertDecimalSetIntrinsic(treeTop, callNode, 4, 4);
                  break;
               case TR::com_ibm_dataaccess_ByteArrayMarshaller_writeDouble_:
                  returnNode = insertDecimalSetIntrinsic(treeTop, callNode, 8, 8);
                  break;

                  // ByteArray Unmarshalling methods
               case TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShort_:
                  returnNode = insertIntegerGetIntrinsic(treeTop, callNode, 2, 2);
                  break;
               case TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShortLength_:
                  returnNode = insertIntegerGetIntrinsic(treeTop, callNode, 0, 2);
                  break;
               case TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readInt_:
                  returnNode = insertIntegerGetIntrinsic(treeTop, callNode, 4, 4);
                  break;
               case TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readIntLength_:
                  returnNode = insertIntegerGetIntrinsic(treeTop, callNode, 0, 4);
                  break;
               case TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLong_:
                  returnNode = insertIntegerGetIntrinsic(treeTop, callNode, 8, 8);
                  break;
               case TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLongLength_:
                  returnNode = insertIntegerGetIntrinsic(treeTop, callNode, 0, 8);
                  break;

               case TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readFloat_:
                  returnNode = insertDecimalGetIntrinsic(treeTop, callNode, 4, 4);
                  break;
               case TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readDouble_:
                  returnNode = insertDecimalGetIntrinsic(treeTop, callNode, 8, 8);
                  break;

               default: break;
                  }

               if (returnNode)
                  {
                  result = 1;
                  matched = true;

                  printInliningStatus(true, callNode);
                  for (int i=callNode->getNumChildren();i>0;i--)
                     callNode->getChild(i-1)->recursivelyDecReferenceCount();
                  callNode->setNumChildren(returnNode->getNumChildren());
                  callNode->setSymbolReference(NULL);
                  TR::Node::recreate(callNode, returnNode->getOpCodeValue());
                  if (callNode->getOpCode().hasSymbolReference())
                     callNode->setSymbolReference(returnNode->getSymbolReference());
                  for (int i=callNode->getNumChildren();i>0;i--)
                     callNode->setChild(i-1, returnNode->getChild(i-1));
                  }
               }

            bool isZLinux = TR::Compiler->target.cpu.isZ() && TR::Compiler->target.isLinux();
            bool isZOS = TR::Compiler->target.isZOS();

            if (!matched && (isZOS || isZLinux) &&
                !comp()->getOption(TR_DisablePackedDecimalIntrinsics))
               {
               matched = true;
               switch (callSymbol->getRecognizedMethod())
                  {
               // DAA Packed Decimal Check
               case TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal_:
                  result |= inlineCheckPackedDecimal(treeTop, callNode); break;

                  // DAA Packed Decimal <-> Unicode Decimal
               case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToUnicodeDecimal_:
                  result |= generatePD2UD(treeTop, callNode, true); break;
               case TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToPackedDecimal_:
                  result |= generateUD2PD(treeTop, callNode, true); break;

                  // DAA Packed Decimal <-> External Decimal
               case TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToPackedDecimal_:
                  result |= generateUD2PD(treeTop, callNode, false); break;
               case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToExternalDecimal_:
                  result |= generatePD2UD(treeTop, callNode, false); break;

               default: matched = false; break;
                  }
               }

            if (!matched && (isZOS || isZLinux) &&
                    !block->isCold() &&
                    !comp()->getOption(TR_DisablePackedDecimalIntrinsics))
               {
               matched = true;
               comp()->cg()->setUpStackSizeForCallNode(callNode);
               switch (callSymbol->getRecognizedMethod())
                  {
               // DAA Packed Decimal arithmetic methods
               case TR::com_ibm_dataaccess_PackedDecimal_addPackedDecimal_:
                  result |= genArithmeticIntrinsic(treeTop, callNode, TR::pdadd); break;
               case TR::com_ibm_dataaccess_PackedDecimal_subtractPackedDecimal_:
                  result |= genArithmeticIntrinsic(treeTop, callNode, TR::pdsub); break;
               case TR::com_ibm_dataaccess_PackedDecimal_multiplyPackedDecimal_:
                  result |= genArithmeticIntrinsic(treeTop, callNode, TR::pdmul); break;
               case TR::com_ibm_dataaccess_PackedDecimal_dividePackedDecimal_:
                  result |= genArithmeticIntrinsic(treeTop, callNode, TR::pddiv); break;
               case TR::com_ibm_dataaccess_PackedDecimal_remainderPackedDecimal_:
                  result |= genArithmeticIntrinsic(treeTop, callNode, TR::pdrem); break;

                  // DAA Packed Decimal shift methods
               case TR::com_ibm_dataaccess_PackedDecimal_shiftLeftPackedDecimal_:
                  result |= genShiftLeftIntrinsic(treeTop, callNode); break;
               case TR::com_ibm_dataaccess_PackedDecimal_shiftRightPackedDecimal_:
                  result |= genShiftRightIntrinsic(treeTop, callNode); break;

                  // DAA Packed Decimal comparison methods
               case TR::com_ibm_dataaccess_PackedDecimal_lessThanPackedDecimal_:
                  result |= genComparisionIntrinsic(treeTop, callNode, TR::pdcmplt); break;
               case TR::com_ibm_dataaccess_PackedDecimal_lessThanOrEqualsPackedDecimal_:
                  result |= genComparisionIntrinsic(treeTop, callNode, TR::pdcmple); break;
               case TR::com_ibm_dataaccess_PackedDecimal_greaterThanPackedDecimal_:
                  result |= genComparisionIntrinsic(treeTop, callNode, TR::pdcmpgt); break;
               case TR::com_ibm_dataaccess_PackedDecimal_greaterThanOrEqualsPackedDecimal_:
                  result |= genComparisionIntrinsic(treeTop, callNode, TR::pdcmpge); break;
               case TR::com_ibm_dataaccess_PackedDecimal_equalsPackedDecimal_:
                  result |= genComparisionIntrinsic(treeTop, callNode, TR::pdcmpeq); break;

                  // DAA Packed Decimal <-> Integer
               case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger_:
                  {
                  if (IS_VARIABLE_PD2I(callNode))
                     {
                     variableCallTreeTops->push_back(treeTop);
                     }
                  else
                     {
                     result |= generatePD2I(treeTop, callNode, true, false);
                     }
                  break;
                  }
               case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger_ByteBuffer_:
                  {
                  if (IS_VARIABLE_PD2I(callNode))
                     {
                     variableCallTreeTops->push_back(treeTop);
                     }
                  else
                     {
                     result |= generatePD2I(treeTop, callNode, true, true);
                     }
                  break;
                  }
               case TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal_:
                  result |= generateI2PD(treeTop, callNode, true, false); break;
               case TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal_ByteBuffer_:
                  result |= generateI2PD(treeTop, callNode, true, true); break;

                  // DAA Packed Decimal <-> Long
               case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong_:
                  {
                  if (IS_VARIABLE_PD2I(callNode))
                     {
                     variableCallTreeTops->push_back(treeTop);
                     }
                  else
                     {
                     result |= generatePD2I(treeTop, callNode, false, false);
                     }
                  break;
                  }
               case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong_ByteBuffer_:
                  {
                  if (IS_VARIABLE_PD2I(callNode))
                     {
                     variableCallTreeTops->push_back(treeTop);
                     }
                  else
                     {
                     result |= generatePD2I(treeTop, callNode, false, true);
                     }
                  break;
                  }
               case TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal_:
                  result |= generateI2PD(treeTop, callNode, false, false); break;
               case TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal_ByteBuffer_:
                  result |= generateI2PD(treeTop, callNode, false, true); break;

               default: matched = false; break;
                  }
               }

            if (matched && result
                && !requestOSRGuardRemoval
                && TR_OSRGuardRemoval::findMatchingOSRGuard(comp(), treeTop))
               requestOSRGuardRemoval = true;

            blockResult |= result;
            }
         }
      }

   // If yields to the VM have been removed, it is possible to remove OSR guards as well
   //
   if (requestOSRGuardRemoval)
      requestOpt(OMR::osrGuardRemoval);

   return blockResult;
   }

bool TR_DataAccessAccelerator::isChildConst(TR::Node* node, int32_t child)
   {
   return node->getChild(child)->getOpCode().isLoadConst();
   }

TR::Node* TR_DataAccessAccelerator::insertDecimalGetIntrinsic(TR::TreeTop* callTreeTop, TR::Node* callNode, int32_t sourceNumBytes, int32_t targetNumBytes)
   {
   if (targetNumBytes != 4 && targetNumBytes != 8)
      {
      printInliningStatus (false, callNode, "targetNumBytes is invalid. Valid targetNumBytes values are 4 or 8.");
      return NULL;
      }

   if (sourceNumBytes != 4 && sourceNumBytes != 8)
      {
      printInliningStatus (false, callNode, "sourceNumBytes is invalid. Valid sourceNumBytes values are 4 or 8.");
      return NULL;
      }

   if (sourceNumBytes > targetNumBytes)
      {
      printInliningStatus (false, callNode, "sourceNumBytes is out of bounds.");
      return NULL;
      }

   TR::Node* byteArrayNode = callNode->getChild(0);
   TR::Node* offsetNode = callNode->getChild(1);
   TR::Node* bigEndianNode = callNode->getChild(2);

   if (!bigEndianNode->getOpCode().isLoadConst())
      {
      printInliningStatus (false, callNode, "bigEndianNode is not constant.");
      return NULL;
      }

   // Determines whether a TR::ByteSwap needs to be inserted before the store to the byteArray
   bool requiresByteSwap = TR::Compiler->target.cpu.isBigEndian() != static_cast <bool> (bigEndianNode->getInt());

   if (requiresByteSwap && !TR::Compiler->target.cpu.isZ())
      {
      printInliningStatus (false, callNode, "Unmarshalling is not supported because ByteSwap IL evaluators are not implemented.");
      return NULL;
      }

   if (performTransformation(comp(), "O^O TR_DataAccessAccelerator: insertDecimalGetIntrinsic on callNode %p\n", callNode))
      {
      insertByteArrayNULLCHK(callTreeTop, callNode, byteArrayNode);

      insertByteArrayBNDCHK(callTreeTop, callNode, byteArrayNode, offsetNode, 0);
      insertByteArrayBNDCHK(callTreeTop, callNode, byteArrayNode, offsetNode, sourceNumBytes - 1);

      TR::DataType sourceDataType = TR::NoType;
      TR::DataType targetDataType = TR::NoType;

      // Default case is impossible due to previous checks
      switch (sourceNumBytes)
         {
         case 4: sourceDataType = TR::Float;  break;
         case 8: sourceDataType = TR::Double; break;
         }

      TR::ILOpCodes op = TR::BadILOp;

      // Default case is impossible due to previous checks
      switch (sourceNumBytes)
         {
         case 4: op = requiresByteSwap ? TR::iriload : TR::floadi; break;
         case 8: op = requiresByteSwap ? TR::irlload : TR::dloadi; break;
         }

      // Default case is impossible due to previous checks
      switch (targetNumBytes)
         {
         case 4: targetDataType = TR::Float;  break;
         case 8: targetDataType = TR::Double; break;
         }

      TR::Node* valueNode = TR::Node::createWithSymRef(op, 1, 1, createByteArrayElementAddress(callTreeTop, callNode, byteArrayNode, offsetNode), comp()->getSymRefTab()->findOrCreateGenericIntShadowSymbolReference(0));

      if (requiresByteSwap)
         {
         // Default case is impossible due to previous checks
         switch (sourceNumBytes)
            {
            case 4: valueNode = TR::Node::create(TR::ibits2f, 1, valueNode); break;
            case 8: valueNode = TR::Node::create(TR::lbits2d, 1, valueNode); break;
            }
         }

      if (sourceNumBytes != targetNumBytes)
         {
         valueNode = TR::Node::create(TR::ILOpCode::getProperConversion(sourceDataType, targetDataType, false), 1, valueNode);
         }

      return valueNode;
      }

   return NULL;
   }

TR::Node* TR_DataAccessAccelerator::insertDecimalSetIntrinsic(TR::TreeTop* callTreeTop, TR::Node* callNode, int32_t sourceNumBytes, int32_t targetNumBytes)
   {
   if (sourceNumBytes != 4 && sourceNumBytes != 8)
      {
      printInliningStatus (false, callNode, "sourceNumBytes is invalid. Valid sourceNumBytes values are 4 or 8.");
      return NULL;
      }

   if (targetNumBytes != 4 && targetNumBytes != 8)
      {
      printInliningStatus (false, callNode, "targetNumBytes is invalid. Valid targetNumBytes values are 4 or 8.");
      return NULL;
      }

   if (targetNumBytes > sourceNumBytes)
      {
      printInliningStatus (false, callNode, "targetNumBytes is out of bounds.");
      return NULL;
      }

   TR::Node* valueNode = callNode->getChild(0);
   TR::Node* byteArrayNode = callNode->getChild(1);
   TR::Node* offsetNode = callNode->getChild(2);
   TR::Node* bigEndianNode = callNode->getChild(3);

   if (!bigEndianNode->getOpCode().isLoadConst())
      {
      printInliningStatus (false, callNode, "bigEndianNode is not constant.");
      return NULL;
      }

   // Determines whether a TR::ByteSwap needs to be inserted before the store to the byteArray
   bool requiresByteSwap = TR::Compiler->target.cpu.isBigEndian() != static_cast <bool> (bigEndianNode->getInt());

   if (requiresByteSwap && !TR::Compiler->target.cpu.isZ())
      {
      printInliningStatus (false, callNode, "Unmarshalling is not supported because ByteSwap IL evaluators are not implemented.");
      return NULL;
      }

   if (performTransformation(comp(), "O^O TR_DataAccessAccelerator: insertDecimalSetIntrinsic on callNode %p\n", callNode))
      {
      insertByteArrayNULLCHK(callTreeTop, callNode, byteArrayNode);

      insertByteArrayBNDCHK(callTreeTop, callNode, byteArrayNode, offsetNode, 0);
      insertByteArrayBNDCHK(callTreeTop, callNode, byteArrayNode, offsetNode, targetNumBytes - 1);

      TR::DataType sourceDataType = TR::NoType;
      TR::DataType targetDataType = TR::NoType;

      // Default case is impossible due to previous checks
      switch (sourceNumBytes)
         {
         case 4: sourceDataType = TR::Float;  break;
         case 8: sourceDataType = TR::Double; break;
         }

      // Default case is impossible due to previous checks
      switch (targetNumBytes)
         {
         case 4: targetDataType = TR::Float;  break;
         case 8: targetDataType = TR::Double; break;
         }

      TR::ILOpCodes op = TR::BadILOp;

      // Default case is impossible due to previous checks
      switch (targetNumBytes)
         {
         case 4: op = requiresByteSwap ? TR::iristore : TR::fstorei; break;
         case 8: op = requiresByteSwap ? TR::irlstore : TR::dstorei; break;
         }

      // Create the proper conversion if the source and target sizes are different
      if (sourceNumBytes != targetNumBytes)
         {
         valueNode = TR::Node::create(TR::ILOpCode::getProperConversion(sourceDataType, targetDataType, false), 1, valueNode);
         }

      if (requiresByteSwap)
         {
         // Default case is impossible due to previous checks
         switch (targetNumBytes)
            {
            case 4: valueNode = TR::Node::create(TR::fbits2i, 1, valueNode); break;
            case 8: valueNode = TR::Node::create(TR::dbits2l, 1, valueNode); break;
            }
         }

      return TR::Node::createWithSymRef(op, 2, 2, createByteArrayElementAddress(callTreeTop, callNode, byteArrayNode, offsetNode), valueNode, comp()->getSymRefTab()->findOrCreateGenericIntShadowSymbolReference(0));
      }

   return NULL;
   }

bool TR_DataAccessAccelerator::inlineCheckPackedDecimal(TR::TreeTop* callTreeTop, TR::Node* callNode)
   {
   TR::Node* byteArrayNode = callNode->getChild(0);
   TR::Node* offsetNode = callNode->getChild(1);
   TR::Node* precisionNode = callNode->getChild(2);
   TR::Node* ignoreHighNibbleForEvenPrecisionNode = callNode->getChild(3);
   TR::Node* canOverwriteHighNibbleForEvenPrecisionNode = callNode->getChild(4);
   int32_t precision = precisionNode->getInt();;
   char* failMsg = NULL;

   if (!precisionNode->getOpCode().isLoadConst())
      failMsg = "precisionNode is not constant.";
   else if(precision < 1 || precision > 31)
      failMsg = "precisionNode is out of bounds.";
   else if (!ignoreHighNibbleForEvenPrecisionNode->getOpCode().isLoadConst())
      failMsg = "ignoreHighNibbleForEvenPrecisionNode is not constant.";
   else if (!canOverwriteHighNibbleForEvenPrecisionNode->getOpCode().isLoadConst())
      failMsg = "canOverwriteHighNibbleForEvenPrecisionNode is not constant.";

   if (failMsg)
      {
      TR::DebugCounter::incStaticDebugCounter(comp(),
                                               TR::DebugCounter::debugCounterName(comp(),
                                                                                  "DAA/rejected/chkPacked"));

      return printInliningStatus (false, callNode, failMsg);
      }

   if (performTransformation(comp(), "O^O TR_DataAccessAccelerator: inlineCheckPackedDecimal on callNode %p\n", callNode))
      {
      TR::DebugCounter::incStaticDebugCounter(comp(),
                                              TR::DebugCounter::debugCounterName(comp(),
                                                                                 "DAA/inlined/chkPacked"));

      insertByteArrayNULLCHK(callTreeTop, callNode, byteArrayNode);

      int32_t precisionSizeInNumberOfBytes = TR::DataType::getSizeFromBCDPrecision(TR::PackedDecimal, precision);

      insertByteArrayBNDCHK(callTreeTop, callNode, byteArrayNode, offsetNode, 0);
      insertByteArrayBNDCHK(callTreeTop, callNode, byteArrayNode, offsetNode, precisionSizeInNumberOfBytes - 1);

      TR::SymbolReference* packedDecimalSymbolReference = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::PackedDecimal, NULL, precisionSizeInNumberOfBytes, fe());

      TR::Node* pdchkChild0Node = TR::Node::createWithSymRef(TR::pdloadi, 1, 1, constructAddressNode(callNode, byteArrayNode, offsetNode), packedDecimalSymbolReference);

      // The size argument passed to create an array shadow symbol reference is the size in number of bytes that this PackedDecimal represents.
      // Unfortunately when a Node is constructed with this symbol reference we extract the size from the symbol reference and convert it to a
      // precision via a helper function. Because this conversion is not injective we may not get back the original precision we calculated
      // above. This is why we must explicitly set the precision on the Node after creation.

      pdchkChild0Node->setDecimalPrecision(precision);

      if (precision % 2 == 0)
         {
         const bool ignoreHighNibbleForEvenPrecision = static_cast <bool> (ignoreHighNibbleForEvenPrecisionNode->getInt());
         const bool canOverwriteHighNibbleForEvenPrecision = static_cast <bool> (canOverwriteHighNibbleForEvenPrecisionNode->getInt());

         if (ignoreHighNibbleForEvenPrecision || canOverwriteHighNibbleForEvenPrecision)
            {
            // Increase the precision of the pdload by 1 to pretend that we have an extra digit, then create a new parent on top of the pdload
            // which will truncate Packed Decimal by modifying its precision to the desired value. This has the effect of creating a new temporary
            // Packed Decimal value which properly ignores the high nibble if the precision is even, and more over it has a value of 0 in the high nibble.

            pdchkChild0Node->setDecimalPrecision(precision + 1);

            pdchkChild0Node = TR::Node::create(TR::pdModifyPrecision, 1, pdchkChild0Node);

            pdchkChild0Node->setDecimalPrecision(precision);

            // If we are allowed to overwrite the high nibble if the precision is even then we need to store temporary Packed Decimal we just
            // created back into the original byte array. We once again pretend that we have an extra digit when doing this store because we also want to
            // store out the extra 0 digit which is guaranteed to be present due to the above computation.

            if (canOverwriteHighNibbleForEvenPrecision)
               {
               int32_t precisionSizeInNumberOfBytes = TR::DataType::getSizeFromBCDPrecision(TR::PackedDecimal, precision + 1);

               TR::SymbolReference* packedDecimalSymbolReference = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::PackedDecimal, NULL, precisionSizeInNumberOfBytes, fe());

               //this node should be inserted after callNode
               TR::Node * pdstoreNode = TR::Node::createWithSymRef(TR::pdstorei, 2, 2, constructAddressNode(callNode, byteArrayNode, offsetNode), pdchkChild0Node, packedDecimalSymbolReference);

               pdstoreNode->setDecimalPrecision(precision + 1);

               callTreeTop->insertAfter(TR::TreeTop::create(comp(), pdstoreNode));
               }
            }
         }

      // We will be recreating the callNode so decrement the reference count of all it's children
      for (auto i = 0; i < callNode->getNumChildren(); ++i)
         {
         callNode->getChild(i)->decReferenceCount();
         }

      TR::Node::recreateWithoutProperties(callNode, TR::pdchk, 1, pdchkChild0Node);

      return true;
      }

   return false;
   }

TR::Node* TR_DataAccessAccelerator::insertIntegerGetIntrinsic(TR::TreeTop* callTreeTop, TR::Node* callNode, int32_t sourceNumBytes, int32_t targetNumBytes)
   {
   if (targetNumBytes != 1 && targetNumBytes != 2 && targetNumBytes != 4 && targetNumBytes != 8)
      {
      printInliningStatus (false, callNode, "targetNumBytes is invalid. Valid targetNumBytes values are 1, 2, 4, or 8.");
      return NULL;
      }

   TR::Node* byteArrayNode = callNode->getChild(0);
   TR::Node* offsetNode = callNode->getChild(1);
   TR::Node* bigEndianNode = callNode->getChild(2);
   TR::Node* numBytesNode = NULL;
   TR::Node* signExtendNode = NULL;

   if (!bigEndianNode->getOpCode().isLoadConst())
      {
      printInliningStatus (false, callNode, "bigEndianNode is not constant.");
      return NULL;
      }

   // Determines whether a TR::ByteSwap needs to be inserted before the store to the byteArray
   bool requiresByteSwap = TR::Compiler->target.cpu.isBigEndian() != static_cast <bool> (bigEndianNode->getInt());

   if (requiresByteSwap && !TR::Compiler->target.cpu.isZ())
      {
      printInliningStatus (false, callNode, "Unmarshalling is not supported because ByteSwap IL evaluators are not implemented.");
      return NULL;
      }

   bool needUnsignedConversion = false;

   // This check indicates that the sourceNumBytes value is specified on the callNode, so we must extract it
   if (sourceNumBytes == 0)
      {
      numBytesNode = callNode->getChild(3);

      if (!numBytesNode->getOpCode().isLoadConst())
         {
         printInliningStatus (false, callNode, "numBytesNode is not constant.");
         return NULL;
         }

      sourceNumBytes = numBytesNode->getInt();

      if (sourceNumBytes != 1 && sourceNumBytes != 2 && sourceNumBytes != 4 && sourceNumBytes != 8)
         {
         printInliningStatus (false, callNode, "sourceNumBytes is invalid. Valid targetNumBytes values are 1, 2, 4, or 8.");
         return NULL;
         }

      if (sourceNumBytes > targetNumBytes)
         {
         printInliningStatus (false, callNode, "sourceNumBytes is out of bounds.");
         return NULL;
         }

      signExtendNode = callNode->getChild(4);

      if (!signExtendNode->getOpCode().isLoadConst())
         {
         printInliningStatus (false, callNode, "signExtendNode is not constant.");
         return NULL;
         }

      needUnsignedConversion = sourceNumBytes < targetNumBytes && static_cast <bool> (signExtendNode->getInt() != 1);
      }
   else
      {
      sourceNumBytes = targetNumBytes;
      }

   if (performTransformation(comp(), "O^O TR_DataAccessAccelerator: genSimpleGetBinary call: %p inlined.\n", callNode))
      {
      insertByteArrayNULLCHK(callTreeTop, callNode, byteArrayNode);

      insertByteArrayBNDCHK(callTreeTop, callNode, byteArrayNode, offsetNode, 0);
      insertByteArrayBNDCHK(callTreeTop, callNode, byteArrayNode, offsetNode, sourceNumBytes - 1);

      TR::DataType sourceDataType = TR::NoType;
      TR::DataType targetDataType = TR::NoType;

      // Default case is impossible due to previous checks
      switch (sourceNumBytes)
         {
         case 1: sourceDataType = TR::Int8;  break;
         case 2: sourceDataType = TR::Int16; break;
         case 4: sourceDataType = TR::Int32; break;
         case 8: sourceDataType = TR::Int64; break;
         }

      TR::ILOpCodes op = TR::BadILOp;

      // Default case is impossible due to previous checks
      switch (sourceNumBytes)
         {
         case 1: op = TR::bloadi; break;
         case 2: op = requiresByteSwap ? TR::irsload : TR::sloadi; break;
         case 4: op = requiresByteSwap ? TR::iriload : TR::iloadi; break;
         case 8: op = requiresByteSwap ? TR::irlload : TR::lloadi; break;
         }

      // Default case is impossible due to previous checks
      switch (targetNumBytes)
         {
         case 1: targetDataType = TR::Int32; break;
         case 2: targetDataType = TR::Int32; break;
         case 4: targetDataType = TR::Int32; break;
         case 8: targetDataType = TR::Int64; break;
         }

      TR::Node* valueNode = TR::Node::createWithSymRef(op, 1, 1, createByteArrayElementAddress(callTreeTop, callNode, byteArrayNode, offsetNode), comp()->getSymRefTab()->findOrCreateGenericIntShadowSymbolReference(0));

      if (sourceDataType != targetDataType)
         {
         valueNode = TR::Node::create(TR::ILOpCode::getProperConversion(sourceDataType, targetDataType, needUnsignedConversion), 1, valueNode);
         }

      return valueNode;
      }

   return NULL;
   }

TR::Node* TR_DataAccessAccelerator::insertIntegerSetIntrinsic(TR::TreeTop* callTreeTop, TR::Node* callNode, int32_t sourceNumBytes, int32_t targetNumBytes)
   {
   if (sourceNumBytes != 1 && sourceNumBytes != 2 && sourceNumBytes != 4 && sourceNumBytes != 8)
      {
      printInliningStatus (false, callNode, "sourceNumBytes is invalid. Valid sourceNumBytes values are 1, 2, 4, or 8.");
      return NULL;
      }

   TR::Node* valueNode = callNode->getChild(0);
   TR::Node* byteArrayNode = callNode->getChild(1);
   TR::Node* offsetNode = callNode->getChild(2);
   TR::Node* bigEndianNode = callNode->getChild(3);
   TR::Node* numBytesNode = NULL;

   if (!bigEndianNode->getOpCode().isLoadConst())
      {
      printInliningStatus (false, callNode, "bigEndianNode is not constant.");
      return NULL;
      }

   // Determines whether a TR::ByteSwap needs to be inserted before the store to the byteArray
   bool requiresByteSwap = TR::Compiler->target.cpu.isBigEndian() != static_cast <bool> (bigEndianNode->getInt());

   if (requiresByteSwap && !TR::Compiler->target.cpu.isZ())
      {
      printInliningStatus (false, callNode, "Marshalling is not supported because ByteSwap IL evaluators are not implemented.");
      return NULL;
      }

   // This check indicates that the targetNumBytes value is specified on the callNode, so we must extract it
   if (targetNumBytes == 0)
      {
      numBytesNode = callNode->getChild(4);

      if (!numBytesNode->getOpCode().isLoadConst())
         {
         printInliningStatus (false, callNode, "numBytesNode is not constant.");
         return NULL;
         }

      targetNumBytes = numBytesNode->getInt();

      if (targetNumBytes != 1 && targetNumBytes != 2 && targetNumBytes != 4 && targetNumBytes != 8)
         {
         printInliningStatus (false, callNode, "targetNumBytes is invalid. Valid targetNumBytes values are 1, 2, 4, or 8.");
         return NULL;
         }

      if (targetNumBytes > sourceNumBytes)
         {
         printInliningStatus (false, callNode, "targetNumBytes is out of bounds.");
         return NULL;
         }
      }
   else
      {
      targetNumBytes = sourceNumBytes;
      }

   if (performTransformation(comp(), "O^O TR_DataAccessAccelerator: genSimplePutBinary call: %p inlined.\n", callNode))
      {
      insertByteArrayNULLCHK(callTreeTop, callNode, byteArrayNode);

      insertByteArrayBNDCHK(callTreeTop, callNode, byteArrayNode, offsetNode, 0);
      insertByteArrayBNDCHK(callTreeTop, callNode, byteArrayNode, offsetNode, targetNumBytes - 1);

      TR::DataType sourceDataType = TR::NoType;
      TR::DataType targetDataType = TR::NoType;

      // Default case is impossible due to previous checks
      switch (sourceNumBytes)
         {
         case 1: sourceDataType = TR::Int32; break;
         case 2: sourceDataType = TR::Int32; break;
         case 4: sourceDataType = TR::Int32; break;
         case 8: sourceDataType = TR::Int64; break;
         }

      // Default case is impossible due to previous checks
      switch (targetNumBytes)
         {
         case 1: targetDataType = TR::Int8;  break;
         case 2: targetDataType = TR::Int16; break;
         case 4: targetDataType = TR::Int32; break;
         case 8: targetDataType = TR::Int64; break;
         }

      TR::ILOpCodes op = TR::BadILOp;

      // Default case is impossible due to previous checks
      switch (targetNumBytes)
         {
         case 1: op = TR::bstorei; break;
         case 2: op = requiresByteSwap ? TR::irsstore : TR::sstorei; break;
         case 4: op = requiresByteSwap ? TR::iristore : TR::istorei; break;
         case 8: op = requiresByteSwap ? TR::irlstore : TR::lstorei; break;
         }

      // Create the proper conversion if the source and target sizes are different
      if (sourceDataType != targetDataType)
         {
         valueNode = TR::Node::create(TR::ILOpCode::getProperConversion(sourceDataType, targetDataType, false), 1, valueNode);
         }

      return TR::Node::createWithSymRef(op, 2, 2, createByteArrayElementAddress(callTreeTop, callNode, byteArrayNode, offsetNode), valueNode, comp()->getSymRefTab()->findOrCreateGenericIntShadowSymbolReference(0));
      }

   return NULL;
   }

TR::Node* TR_DataAccessAccelerator::constructAddressNode(TR::Node* callNode, TR::Node* arrayNode, TR::Node* offsetNode)
   {
   TR::Node * arrayAddressNode;
   TR::Node * headerConstNode;
   TR::Node * totalOffsetNode;

   TR::Node * pdBufAddressNode = NULL;
   TR::Node * pdBufPositionNode = NULL;


   if (callNode->getSymbol()->getResolvedMethodSymbol())
      {
      if (callNode->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod())
         {
         bool isByteBuffer = false;

         if ((callNode->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal_ByteBuffer_)
               || (callNode->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal_ByteBuffer_))
            {
            isByteBuffer = true;
            pdBufAddressNode = callNode->getChild(5);
            pdBufPositionNode = callNode->getChild(7);
            }
         else if ((callNode->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger_ByteBuffer_)
               || (callNode->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong_ByteBuffer_))
            {
            isByteBuffer = true;
            pdBufAddressNode = callNode->getChild(4);
            pdBufPositionNode = callNode->getChild(6);
            }

         if (isByteBuffer)
            {
            TR::Node* offset = TR::Node::create(TR::i2l, 1, TR::Node::create(TR::iadd, 2, pdBufPositionNode, offsetNode));
            TR::Node* address = TR::Node::create(TR::ladd, 2, pdBufAddressNode, offset);
            return TR::Node::create(TR::l2a, 1, address);
            }
         }
      }

   if (TR::Compiler->target.is64Bit())
      {
      headerConstNode = TR::Node::create(callNode, TR::lconst, 0, 0);
      headerConstNode->setLongInt(TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
      totalOffsetNode = TR::Node::create(TR::ladd, 2, headerConstNode, TR::Node::create(TR::i2l, 1, offsetNode));
      arrayAddressNode = TR::Node::create(TR::aladd, 2, arrayNode, totalOffsetNode);
      }
   else
      {
      headerConstNode = TR::Node::create(callNode, TR::iconst, 0,
            TR::Compiler->om.contiguousArrayHeaderSizeInBytes());

      totalOffsetNode = TR::Node::create(TR::iadd, 2, headerConstNode, offsetNode);
      arrayAddressNode = TR::Node::create(TR::aiadd, 2, arrayNode, totalOffsetNode);
      }
   arrayAddressNode->setIsInternalPointer(true);
   return arrayAddressNode;
   }

bool TR_DataAccessAccelerator::genComparisionIntrinsic(TR::TreeTop* treeTop, TR::Node* callNode, TR::ILOpCodes ops)
   {
   if (!isChildConst(callNode, 2) || !isChildConst(callNode, 5))
      {
      return printInliningStatus(false, callNode, "Child (2|5) is not constant");
      }

   TR_ASSERT(callNode->getNumChildren() == 6, "Expecting BCD cmp call with 6 children.");

   TR::Node * op1Node = callNode->getChild(0);
   TR::Node * offset1Node = callNode->getChild(1);
   TR::Node * prec1Node = callNode->getChild(2);
   TR::Node * op2Node = callNode->getChild(3);
   TR::Node * offset2Node = callNode->getChild(4);
   TR::Node * prec2Node = callNode->getChild(5);

   int precision1 = prec1Node->getInt();
   int precision2 = prec2Node->getInt();

   if (precision1 > 31 || precision2 > 31 || precision1 < 1 || precision2 < 1)
      {
      return printInliningStatus(false, callNode, "Invalid precisions. Valid precisions are in range [1, 31]");
      }

   if (!performTransformation(comp(), "O^O TR_DataAccessAccelerator: genComparison call: %p, Comparison type: %d inlined.\n", callNode, ops))
      {
      return false;
      }

   //create loading
   // loading The first operand
   TR::Node * arrayAddressNode1 = constructAddressNode(callNode, op1Node, offset1Node);
   TR::SymbolReference * symRef1 = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::PackedDecimal, arrayAddressNode1, 8, fe());
   symRef1->setOffset(0);

   TR::Node * pdload1 = TR::Node::create(TR::pdloadi, 1, arrayAddressNode1);
   pdload1->setSymbolReference(symRef1);
   pdload1->setDecimalPrecision(precision1);

   //load the second operand
   TR::Node * arrayAddressNode2 = constructAddressNode(callNode, op2Node, offset2Node);
   TR::SymbolReference * symRef2 = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::PackedDecimal, arrayAddressNode2, 8, fe());
   symRef2->setOffset(0);

   TR::Node * pdload2 = TR::Node::create(TR::pdloadi, 1, arrayAddressNode2);
   pdload2->setSymbolReference(symRef2);
   pdload2->setDecimalPrecision(precision2);

   //create the BCDCHK:
   TR::Node * pdOpNode = callNode;
   TR::SymbolReference* bcdChkSymRef = callNode->getSymbolReference();
   TR::Node * bcdchkNode = TR::Node::createWithSymRef(TR::BCDCHK, 7, 7,
                                                      pdOpNode,
                                                      callNode->getChild(0), callNode->getChild(1),
                                                      callNode->getChild(2), callNode->getChild(3),
                                                      callNode->getChild(4), callNode->getChild(5),
                                                      bcdChkSymRef);

   pdOpNode->setNumChildren(2);
   pdOpNode->setAndIncChild(0, pdload1);
   pdOpNode->setAndIncChild(1, pdload2);
   pdOpNode->setSymbolReference(NULL);

   // Set inlined site index to make sure AOT TR_RelocationRecordConstantPool::computeNewConstantPool() API can
   // correctly compute a new CP to relocate DAA OOL calls.
   bcdchkNode->setInlinedSiteIndex(callNode->getInlinedSiteIndex());

   //instead of creating comparison operation, re use the callNode:
   TR::Node::recreate(pdOpNode, ops);

   treeTop->setNode(bcdchkNode);

   pdOpNode->decReferenceCount();
   op1Node->decReferenceCount();
   op2Node->decReferenceCount();
   offset1Node->decReferenceCount();
   offset2Node->decReferenceCount();
   prec1Node->decReferenceCount();
   prec2Node->decReferenceCount();

   return printInliningStatus(true, callNode);
   }

bool TR_DataAccessAccelerator::generateI2PD(TR::TreeTop* treeTop, TR::Node* callNode, bool isI2PD, bool isByteBuffer)
   {
   int precision = callNode->getChild(3)->getInt();
   char* failMsg = NULL;

   if (!isChildConst(callNode, 3) || !isChildConst(callNode, 4))
      failMsg = "Child (3|4) is not constant";
   else if (precision < 1 || precision > 31)
      failMsg = "Invalid precision. Valid precision is in range [1, 31]";

   if (failMsg)
      {
      TR::DebugCounter::incStaticDebugCounter(comp(),
                                              TR::DebugCounter::debugCounterName(comp(),
                                                                                 "DAA/rejected/%s",
                                                                                 isI2PD ? "i2pd" : "l2pd"));
      return printInliningStatus(false, callNode, failMsg);
      }

   TR::Node* intNode = NULL;
   TR::Node* pdNode = NULL;
   TR::Node* offsetNode = NULL;
   TR::Node* precNode = NULL;
   TR::Node* errorCheckingNode = NULL;

   // Backing storage info for ByteBuffer
   TR::Node * pdBufAddressNodeCopy = NULL;
   TR::Node * pdBufCapacityNode = NULL;
   TR::Node * pdBufPositionNode = NULL;

   TR::TreeTop *slowPathTreeTop = NULL;
   TR::TreeTop *fastPathTreeTop = NULL;
   TR::Node *slowPathNode = NULL;

   bool needsBCDCHK = (isI2PD && (precision < 10)) || (!isI2PD && (precision < 19));

   //still need to check bounds of pdNode
   if (performTransformation(comp(), "O^O TR_DataAccessAccelerator: %s call: %p inlined.\n", (isI2PD)?"generateI2PD":"generateL2PD", callNode))
      {
      TR::DebugCounter::incStaticDebugCounter(comp(),
                                              TR::DebugCounter::debugCounterName(comp(),
                                                                                 "DAA/inlined/%s",
                                                                                 isI2PD ? "i2pd" : "l2pd"));

      if (isByteBuffer)
         {
         /* We will be creating a precision diamond for the fast / slow path and eliminating the original call.
          * Because we are about to split the CFG we would have to store the original parameters of the call into
          * temp slots as we will be duplicating the call node in the precision diamond but we don't need to since
          * createConditionalBlocksBeforeTree takes care of it. createConditionalBlocksBeforeTree calls block::split
          * with true for the option fixupCommoning and so it will break the commoning and add any necessary temps for you.
          */

         pdBufAddressNodeCopy = TR::Node::copy(callNode->getChild(5));
         pdBufAddressNodeCopy->setReferenceCount(0);
         pdBufCapacityNode = callNode->getChild(6);
         pdBufPositionNode = callNode->getChild(7);
         }

      intNode = callNode->getChild(0);
      pdNode  = callNode->getChild(1);
      offsetNode = callNode->getChild(2);
      precNode = callNode->getChild(3);
      errorCheckingNode = callNode->getChild(4);

      //create a TR::i2pd node and an pdstore.
      //this will not cause an exception, so it is safe to remove BCDCHK
      TR::Node * i2pdNode = TR::Node::create((isI2PD)?TR::i2pd:TR::l2pd, 1, intNode);
      i2pdNode->setDecimalPrecision(precision);

      /**
       * Create separate address nodes for BCDCHK and pdstorei because BCDCHK can GC-and-Return like a call.
       *
       * Having separate address nodes also allows AddrNode2 and AddrNode3 commoning, which then makes
       * copy propagations possible.
       *
       * AddrNode1 could still be commoned with address nodes before the BCDCHK. Hence, the need for
       * UncommonBCDCHKAddressNode codegen pass. AddrNode1 is special in that it has to be rematerialized and used
       * at the end of the BCDCHK OOL path's GC point. No commoning of this node should happen.
       *
       * Example:
       *
       * BCDCHK
       *   pdshlOverflow <prec=9 (len=5) adj=0 round=0>
       *     ....
       *   aladd (internalPtr sharedMemory )                                    AddrNode1
       *     ==>newarray
       *     lconst 8 (highWordZero X!=0 X>=0 )
       *     ....
       * pdstorei  <array-shadow>[#490  Shadow] <prec=9 (len=5)>
       *   aladd (internalPtr sharedMemory )                                    AddrNode2
       *     ==>newarray
       *     ==>lconst 8
       *   ==>pdshlOverflow <prec=9 (len=5)
       * zdsleStorei  <array-shadow>[#492  Shadow]  <prec=9 (len=9)>
       *   ....
       *   zd2zdsle <prec=9 (len=9)>
       *     pd2zd <prec=9 (len=9)>
       *       pdloadi <prec=9 (len=5) adj=0 round=0>
       *         aladd (internalPtr sharedMemory )                              AddrNode3
       *           ==>newarray
       *           ==>lconst 8
       *
       * In the example above, AddrNode 1 to 3 have the same children.
       *
       * AddrNode1, the second child of the BCDCHK node, is meant to be rematerialized for OOL post-call data copy back.
       * See BCDCHKEvaluatorImpl() for BCDCHK tree structure and intended use of its children.
       *
       * 'outOfLineCopyBackAddr' and 'storeAddressNode' correspond to AddrNode1 and AddrNode2, respectively. They
       * are created as separate nodes so that LocalCSE is able to common up AddrNode2 and AddrNode3. If
       * AddrNode1 and AddrNode2 were the same node, the LocalCSE would not consider AddrNode1 an alternative replacement
       * of AddrNode3 because the BCDCHK's symbol canGCAndReturn().
       *
       * With AddrNode2 and AddrNode3 commoned up, the LocalCSE is able to copy propagate pdshlOverflow to the the pd2zd
       * tree and replace its pdloadi.
       */
      TR::Node * outOfLineCopyBackAddr = constructAddressNode(callNode, pdNode, offsetNode);
      TR::Node * storeAddressNode      = constructAddressNode(callNode, pdNode, offsetNode);

      TR::TreeTop * nextTT = treeTop->getNextTreeTop();
      TR::TreeTop * prevTT = treeTop->getPrevTreeTop();

      TR::ILOpCodes op = comp()->il.opCodeForIndirectStore(TR::PackedDecimal);

      TR::Node * pdstore = NULL;
      TR::Node * bcdchkNode = NULL;
      if (needsBCDCHK)
         {
         i2pdNode->setDecimalPrecision((isI2PD)? 10:19);
         TR::Node * pdshlNode = TR::Node::create(TR::pdshlOverflow, 2, i2pdNode, TR::Node::create(callNode, TR::iconst, 0));
         pdshlNode->setDecimalPrecision(precision);

         /* Attaching all the original callNode's children as the children to BCDCHK.
          * We don't want to attach the callNode as a child to BCDCHK since it would be an aberration to the
          * definition of a BCDCHK node. BCDCHK node is already a special type of node, and all optimizations expect the
          * call (i2pd) to be inside the first child of BCDCHK. Attaching another call could cause many things to
          * break as all optimizations such as Value Propagation don't expect it to be there. Attaching the callNode's children
          * to BCDCHK would be safe. We would whip up the call with these attached children during codegen
          * for the fallback of the fastpath.
          */
         TR::SymbolReference* bcdChkSymRef = callNode->getSymbolReference();

         if (isByteBuffer)
            {
            bcdchkNode = TR::Node::createWithSymRef(TR::BCDCHK, 10, 10,
                                                    pdshlNode, outOfLineCopyBackAddr,
                                                    callNode->getChild(0), callNode->getChild(1),
                                                    callNode->getChild(2), callNode->getChild(3),
                                                    callNode->getChild(4), callNode->getChild(5),
                                                    callNode->getChild(6), callNode->getChild(7),
                                                    bcdChkSymRef);
            }
         else
            {
            bcdchkNode = TR::Node::createWithSymRef(TR::BCDCHK, 7, 7,
                                                    pdshlNode, outOfLineCopyBackAddr,
                                                    callNode->getChild(0), callNode->getChild(1),
                                                    callNode->getChild(2), callNode->getChild(3),
                                                    callNode->getChild(4),
                                                    bcdChkSymRef);
            }

         // Set inlined site index to make sure AOT TR_RelocationRecordConstantPool::computeNewConstantPool() API can
         // correctly compute a new CP to relocate DAA OOL calls.
         bcdchkNode->setInlinedSiteIndex(callNode->getInlinedSiteIndex());

         pdstore = TR::Node::create(op, 2, storeAddressNode, pdshlNode);
         }
      else
         {
         pdstore = TR::Node::create(op, 2, storeAddressNode, i2pdNode);
         }

      TR::TreeTop* pdstoreTT = TR::TreeTop::create(comp(), pdstore);

      if (isByteBuffer)
         {
         TR::CFG *cfg = comp()->getFlowGraph();
         TR::Block *callBlock = treeTop->getEnclosingBlock();

         // Generate the slow path
         slowPathTreeTop = TR::TreeTop::create(comp(),treeTop->getNode()->duplicateTree());
         slowPathNode = slowPathTreeTop->getNode()->getFirstChild();

         // Generate the tree to check if the ByteBuffer has a valid address or not
         TR::Node* nullNode = TR::Node::create(TR::lconst, 0, 0);
         TR::Node *isValidAddrNode = TR::Node::createif(TR::iflcmpeq, pdBufAddressNodeCopy, nullNode, treeTop);
         TR::TreeTop *isValidAddrTreeTop = TR::TreeTop::create(comp(), isValidAddrNode, NULL, NULL);

         fastPathTreeTop = pdstoreTT;

         /* Create the diamond in CFG
          * if (ByteBuffer.address != NULL)
          *    fastpath (CVD instruction executed by HW)
          * else
          *    slowpath (call to Java method: convertIntegerToPackedDecimal_)
          * */
         callBlock->createConditionalBlocksBeforeTree(treeTop, isValidAddrTreeTop, slowPathTreeTop, fastPathTreeTop, cfg, false, true);
         }

      TR::SymbolReference * symRef = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::PackedDecimal, outOfLineCopyBackAddr, 8, fe());
      pdstore->setSymbolReference(symRef);
      pdstore->setDecimalPrecision(precision);

      TR::TreeTop* bcdchktt = NULL;
      if (needsBCDCHK)
         bcdchktt = TR::TreeTop::create(comp(), bcdchkNode);

      if (isByteBuffer)
         {
         // the original call will be deleted by createConditionalBlocksBeforeTree, but if the refcount was > 1, we need to insert stores.
         if (needsBCDCHK)
            {
            pdstoreTT->insertBefore(bcdchktt);
            }
         }
      else
         {
         if (needsBCDCHK)
            {
            prevTT->join(bcdchktt);
            bcdchktt->join(pdstoreTT);
            }
         else
            {
            prevTT->join(pdstoreTT);
            }
         pdstoreTT->join(nextTT);

         // we'll be removing the callNode, update its refcount.
         callNode->recursivelyDecReferenceCount();
         }

      return true;
      }
   return false;
   }

void TR_DataAccessAccelerator::createPrecisionDiamond(TR::Compilation* comp,
                                                      TR::TreeTop* treeTop,
                                                      TR::TreeTop* fastTree,
                                                      TR::TreeTop* slowTree,
                                                      bool isPD2I,
                                                      uint32_t numPrecisionNodes,
                                                      ...)
   {
   // Create precision guards
   const uint8_t precisionMin = 1;
   const uint8_t precisionMax = isPD2I ? 15 : 31;

   uint32_t numGuards = numPrecisionNodes * 2;

   TR::StackMemoryRegion stackMemoryRegion(*(comp->trMemory()));

   BlockContainer testBlocks(stackMemoryRegion);
   TreeTopContainer testTTs(stackMemoryRegion);

   va_list precisionNodeList;
   va_start(precisionNodeList, numPrecisionNodes);
   for(uint32_t i = 0; i < numPrecisionNodes; ++i)
      {
      TR::Node* precisionNode = va_arg(precisionNodeList, TR::Node*);
      TR_ASSERT(precisionNode, "Precision node should not be null");
      TR::Node* node1 = TR::Node::createif(TR::ificmpgt, precisionNode->duplicateTree(), TR::Node::iconst(precisionMax));
      TR::Node* node2 = TR::Node::createif(TR::ificmplt, precisionNode->duplicateTree(), TR::Node::iconst(precisionMin));

      testTTs.push_back(TR::TreeTop::create(comp, node1));
      testTTs.push_back(TR::TreeTop::create(comp, node2));
      }
   va_end(precisionNodeList);

   // Split blocks, 1 for each precision test block
   TR::CFG* cfg = comp->getFlowGraph();

   // We will be updating the CFG so invalidate the structure
   cfg->setStructure(0);

   testBlocks.push_back(treeTop->getEnclosingBlock(false));
   for(uint32_t i = 1; i < numGuards; ++i)
      {
      testBlocks.push_back(testBlocks[i-1]->split(treeTop, cfg, true));
      }

   TR::Block* firstTestBlock = testBlocks.front();
   TR::Block* lastTestBlock  = testBlocks.back();

   // This block will contain everything AFTER tree
   TR::Block * otherBlock = lastTestBlock->split(treeTop, cfg, true);

   // Append tree tops
   for(int i = 0; i < numGuards; ++i)
      {
      testBlocks[i]->append(testTTs[i]);
      }

   TR::Node* node = treeTop->getNode();

   // Remove the original tree from the other block
   node->removeAllChildren();

   TR::TreeTop* prevTT = treeTop->getPrevTreeTop();
   TR::TreeTop* nextTT = treeTop->getNextTreeTop();

   prevTT->join(nextTT);

   TR::Block * fastBlock = TR::Block::createEmptyBlock(node, comp, firstTestBlock->getFrequency());
   TR::Block * slowBlock = TR::Block::createEmptyBlock(node, comp, UNKNOWN_COLD_BLOCK_COUNT);

   TR::TreeTop* slowEntry = slowBlock->getEntry();
   TR::TreeTop* fastEntry = fastBlock->getEntry();
   TR::TreeTop* slowExit  = slowBlock->getExit();
   TR::TreeTop* fastExit  = fastBlock->getExit();

   // Fast block is a fall-through of the second if test
   lastTestBlock->getExit()->join(fastEntry);

   cfg->addNode(fastBlock);
   cfg->addNode(slowBlock);

   TR::Block * bestBlock = otherBlock;

   // Find the best place for the slow block
   while (bestBlock && bestBlock->canFallThroughToNextBlock())
      {
      bestBlock = bestBlock->getNextBlock();
      }

   if (bestBlock)
      {
      TR::TreeTop* bestExit = bestBlock->getExit();
      TR::TreeTop* bestNext = bestBlock->getExit()->getNextTreeTop();

      bestExit->join(slowEntry);
      slowExit->join(bestNext);
      }
   else
      {
      cfg->findLastTreeTop()->join(slowBlock->getEntry());
      }

   fastBlock->append(fastTree);
   slowBlock->append(slowTree);

   // Jump back to other block after slow path
   slowBlock->append(TR::TreeTop::create(comp, TR::Node::create(node, TR::Goto, 0, otherBlock->getEntry())));
   for(int i = 0; i < numGuards; ++i)
      {
      testTTs[i]->getNode()->setBranchDestination(slowEntry);
      }

   // Other block is a fall-through of the fast block
   fastExit->join(otherBlock->getEntry());

   cfg->addEdge(TR::CFGEdge::createEdge(lastTestBlock,  fastBlock, trMemory()));
   cfg->addEdge(TR::CFGEdge::createEdge(fastBlock,   otherBlock, trMemory()));
   cfg->addEdge(TR::CFGEdge::createEdge(slowBlock,   otherBlock, trMemory()));
   for(int i = 0; i < numGuards; ++i)
      {
      cfg->addEdge(TR::CFGEdge::createEdge(testBlocks[i], slowBlock, trMemory()));
      }

   // We introduced fastBlock in between these two, so it is not needed anymore
   cfg->removeEdge(lastTestBlock, otherBlock);

   fastBlock->setIsExtensionOfPreviousBlock(false);
   slowBlock->setIsExtensionOfPreviousBlock(false);
   otherBlock->setIsExtensionOfPreviousBlock(false);

   cfg->copyExceptionSuccessors(firstTestBlock, fastBlock);
   cfg->copyExceptionSuccessors(firstTestBlock, slowBlock);
   }

bool
TR_DataAccessAccelerator::generatePD2IConstantParameter(TR::TreeTop* treeTop, TR::Node* callNode, bool isPD2i, bool isByteBuffer)
   {
   TR::Node* pdInputNode    = callNode->getChild(0);
   TR::Node* offsetNode     = callNode->getChild(1);
   TR::Node* precisionNode  = callNode->getChild(2);
   TR::Node* overflowNode   = callNode->getChild(3);

   // Backing storage info for ByteBuffer
   TR::Node * pdBufAddressNodeCopy = NULL;
   TR::Node * pdBufCapacityNode = NULL;
   TR::Node * pdBufPositionNode = NULL;

   TR::TreeTop *slowPathTreeTop = NULL;
   TR::TreeTop *fastPathTreeTop = NULL;
   TR::Node *slowPathNode = NULL;
   TR::Node *pd2iNode = NULL;
   TR::TreeTop *copiedCallNodeTreeTop = NULL;
   TR::Node *copiedCallNode = NULL;
   TR::SymbolReference *newSymbolReference = 0;
   TR::TreeTop *bcdchkTreeTop = NULL;
   int precision = precisionNode->getInt();
   int overflow = overflowNode->getInt();
   char* failMsg = NULL;

   if (precision < 1)
      failMsg = "Invalid precision. Precision can not be less than 1";
   else if (isPD2i && precision > 10)
      failMsg = "Invalid precision. Precision can not be greater than 10";
   else if (!isPD2i && precision > 31)
      failMsg = "Invalid precision. Precision can not be greater than 31";

   if (failMsg)
      {
      TR::DebugCounter::incStaticDebugCounter(comp(),
                                              TR::DebugCounter::debugCounterName(comp(),
                                                                                 "DAA/rejected/%s",
                                                                                 isPD2i ? "pd2i" : "pd2l"));
      return printInliningStatus(false, callNode, failMsg);
      }

   if (performTransformation(comp(), "O^O TR_DataAccessAccelerator: %s call: %p inlined.\n", (isPD2i)?"generatePD2I":"generatePD2L", callNode))
      {
      TR::DebugCounter::incStaticDebugCounter(comp(),
                                              TR::DebugCounter::debugCounterName(comp(),
                                                                                 "DAA/inlined/%s",
                                                                                 isPD2i ? "pd2i" : "pd2l"));
      if (isByteBuffer)
         {
         /* We will be creating a precision diamond for the fast / slow path and eliminating the original call.
          * Because we are about to split the CFG we would have to store the original parameters of the call into
          * temp slots as we will be duplicating the call node in the precision diamond but we don't need to since
          * createConditionalBlocksBeforeTree takes care of it. createConditionalBlocksBeforeTree calls block::split
          * with true for the option fixupCommoning and so it will break the commoning and add any necessary temps for you.
          */

         pdBufAddressNodeCopy = TR::Node::copy(callNode->getChild(4));
         pdBufAddressNodeCopy->setReferenceCount(0);
         pdBufCapacityNode = callNode->getChild(5);
         pdBufPositionNode = callNode->getChild(6);
         }

      //create the packed decimals

      TR::Node * arrayAddressNode = constructAddressNode(callNode, pdInputNode, offsetNode);

      int32_t size = TR::DataType::getSizeFromBCDPrecision(TR::PackedDecimal, precision) - 1;
      TR::SymbolReference * symRef = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::PackedDecimal, arrayAddressNode, 8, fe());
      TR::Node * pdload = TR::Node::create(TR::pdloadi, 1, arrayAddressNode);
      pdload->setSymbolReference(symRef);
      pdload->setDecimalPrecision(precision);

      TR::TreeTop * prevTT = treeTop->getPrevTreeTop();
      TR::TreeTop * nextTT = treeTop->getNextTreeTop();
      TR::Node *bcdchk = NULL;
      TR::SymbolReference* bcdChkSymRef = callNode->getSymbolReference();

      /* Attaching all the original callNode's children as the children to BCDCHK.
       * We don't want to attach the callNode as a child to BCDCHK since it would be an aberration to the
       * definition of a BCDCHK node. BCDCHK node is already a special type of node, and all optimizations expect the
       * call (pd2i) to be in its first child. Attaching another call could cause many things to break as all
       * optimizations such as Value Propagation don't expect it to be there. Attaching the callNode's children
       * to BCDCHK would be safe. We would whip up the call with these attached children during codegen
       * for the fallback of the fastpath.
       */
      if (isByteBuffer)
         {
         // Tree with pd2i() under BCDCHK node
         copiedCallNodeTreeTop = TR::TreeTop::create(comp(),treeTop->getNode()->duplicateTree());
         copiedCallNode = copiedCallNodeTreeTop->getNode()->getFirstChild();
         bcdchk = TR::Node::createWithSymRef(TR::BCDCHK, 8, 8,
                                             copiedCallNode,
                                             callNode->getChild(0), callNode->getChild(1),
                                             callNode->getChild(2), callNode->getChild(3),
                                             callNode->getChild(4), callNode->getChild(5),
                                             callNode->getChild(6),
                                             bcdChkSymRef);

         /*
          * BCDCHK would look something like this for ByteBuffer:
          *
          * n2975n    BCDCHK [#959] ()
            n2958n      pd2i ()
            n2949n        pdloadi  <array-shadow>[#497  Shadow] [flags 0x80000618 0x0 ] <prec=10 (len=6) adj=0> sign=hasState
            n2948n          l2a
            n2947n            ladd
            n2941n              lload  <temp slot 29>[#949  Auto] [flags 0x4 0x0 ]     // address of ByteBuffer
            n2946n              i2l
            n2945n                iadd
            n2943n                  ==>iload                                           // position
            n2938n                  ==>iload                                           // offset
            n2937n      ==>aload                                                       // ByteBuffer
            n2938n      ==>iload                                                       // offset
            n2939n      iload  <temp slot 27>[#947  Auto] [flags 0x3 0x0 ]             // precision
            n2940n      iload  <temp slot 28>[#948  Auto] [flags 0x3 0x0 ]             // checkOverflow
            n2941n      ==>lload                                                       // address of ByteBuffer
            n2942n      ==>iload                                                       // capacity of ByteBuffer
            n2943n      ==>iload                                                       // position of ByteBuffer
          */
         }
      else
         {
         bcdchk = TR::Node::createWithSymRef(TR::BCDCHK, 5, 5,
                                             callNode,
                                             callNode->getChild(0), callNode->getChild(1),
                                             callNode->getChild(2), callNode->getChild(3),
                                             bcdChkSymRef);
         /*
          * BCDCHK would look something like this for byte[]:
          *
          * n2937n    BCDCHK [#990] ()
            n990n       pd2i ()
            n2929n        pdloadi  <array-shadow>[#486  Shadow] [flags 0xffffffff80000612 0x0 ] <prec=10 (len=6) adj=0> sign=hasState   vn=- sti=- udi=- nc=1
            n2928n          aladd (internalPtr sharedMemory )
            n986n             ==>aload
            n2927n            aladd
            n2925n              lconst 8
            n2926n              i2l
            n1001n                ==>iconst 0
            n986n       ==>aload                                                       // byte[]
            n1001n      ==>iconst 0                                                    // offset
            n1002n      iconst 10 (X!=0 X>=0 )                                         // precision
            n1003n      iconst 0 (X==0 X>=0 X<=0 )                                     // checkOverflow
          */
         }

      // Set inlined site index to make sure AOT TR_RelocationRecordConstantPool::computeNewConstantPool() API can
      // correctly compute a new CP to relocate DAA OOL calls.
      bcdchk->setInlinedSiteIndex(callNode->getInlinedSiteIndex());

      TR::DataType dataType = callNode->getDataType();
      if (isByteBuffer)
         {
         TR::CFG *cfg = comp()->getFlowGraph();
         TR::Block *callBlock = treeTop->getEnclosingBlock();

         // Generate the slow path
         slowPathTreeTop = TR::TreeTop::create(comp(),treeTop->getNode()->duplicateTree());
         slowPathNode = slowPathTreeTop->getNode()->getFirstChild();

         // Generate the tree to check if the ByteBuffer has a valid address or not
         TR::Node* nullNode = TR::Node::create(TR::lconst, 0, 0);
         TR::Node *isValidAddrNode = TR::Node::createif(TR::iflcmpeq, pdBufAddressNodeCopy, nullNode, treeTop);
         TR::TreeTop *isValidAddrTreeTop = TR::TreeTop::create(comp(), isValidAddrNode, NULL, NULL);

         bcdchkTreeTop = TR::TreeTop::create(comp(), bcdchk);
         fastPathTreeTop = bcdchkTreeTop;

         if (callNode->getReferenceCount() > 1)
            {
            newSymbolReference = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), dataType);
            TR::Node::recreate(callNode, comp()->il.opCodeForDirectLoad(dataType));
            callNode->setSymbolReference(newSymbolReference);
            callNode->removeAllChildren();
            }

         callNode = copiedCallNode;

         /* Create the diamond in CFG
          * if (ByteBuffer.address != NULL)
          *    fastpath (CVB instruction executed by HW)
          * else
          *    slowpath (call to Java method: convertPackedDecimalToInteger_)
          * */
         callBlock->createConditionalBlocksBeforeTree(treeTop, isValidAddrTreeTop, slowPathTreeTop, fastPathTreeTop, cfg, false, true);
         }

      // we'll be removing the callNode, update its refcount before replacing its fields.
      // The callNode may have more than 1 reference (treetop and i/lstore), so we need to scan through its list of children.
      for (int32_t childCount = callNode->getNumChildren()-1; childCount >= 0; childCount--)
         callNode->getChild(childCount)->recursivelyDecReferenceCount();

      // Replacing TT with BCDCHK, so losing one reference.
      callNode->decReferenceCount();

      //create pd2i:
      pd2iNode = callNode;
      pd2iNode->setNumChildren(1);
      pd2iNode->setAndIncChild(0, pdload);
      if (!isByteBuffer)
         {
         pd2iNode->setSymbolReference(NULL);
         }

      if (isPD2i)
         {
         if (!overflow)
            TR::Node::recreate(pd2iNode, TR::pd2i);
         else
            TR::Node::recreate(pd2iNode, TR::pd2iOverflow);
         }
      else
         {
         if (!overflow)
            TR::Node::recreate(pd2iNode, TR::pd2l);
         else
            TR::Node::recreate(pd2iNode, TR::pd2lOverflow);
         }


      if (isByteBuffer)
         {
         // the original call will be deleted by createConditionalBlocksBeforeTree, but if the refcount was > 1, we need to insert stores.

         if (newSymbolReference)
            {
            /* Storing the result to a temp slot so that it can be loaded from there later
             * We would need to store the result to the same temp slot for both fast and slow path so we that
             * we get the same result irrespective of the path taken.
             * For slowpath: storing the result of icall to temp slot
             * For fastpath: storing the result of pd2i() to temp slot
             */

            TR::Node *slowStoreNode = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(dataType), 1, 1, slowPathTreeTop->getNode()->getFirstChild(), newSymbolReference);
            TR::TreeTop *slowStoreTree = TR::TreeTop::create(comp(), slowStoreNode);

            slowPathTreeTop->insertAfter(slowStoreTree);

            treeTop->setNode(bcdchk);
            TR::Node *fastStoreNode = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(dataType), 1, 1, bcdchkTreeTop->getNode()->getFirstChild(), newSymbolReference);
            TR::TreeTop *fastStoreTree = TR::TreeTop::create(comp(), fastStoreNode);

            bcdchkTreeTop->insertAfter(fastStoreTree);
            }
         else
            {
            treeTop->setNode(bcdchk);
            }
         }
      else
         {
         treeTop->setNode(bcdchk);
         prevTT->join(treeTop);
         treeTop->join(nextTT);
         }

      return true;
      }
   return false;
   }

TR::Node*
TR_DataAccessAccelerator::restructureVariablePrecisionCallNode(TR::TreeTop* treeTop, TR::Node* callNode)
   {
   uint32_t numCallParam = callNode->getNumChildren();
   TR::SymbolReferenceTable*    symRefTab = comp()->getSymRefTab();
   TR::ResolvedMethodSymbol*    methodSym = comp()->getMethodSymbol();

   for(uint32_t i = 0; i < numCallParam; ++i)
      {
      TR::Node* child = callNode->getChild(i);
      TR::SymbolReference* newTemp = symRefTab->createTemporary(methodSym, child->getDataType());
      treeTop->insertBefore(TR::TreeTop::create(comp(), TR::Node::createStore(newTemp, child)));
      child->decReferenceCount();
      callNode->setAndIncChild(i, TR::Node::createLoad(child, newTemp));
      }

   return callNode;
   }

bool
TR_DataAccessAccelerator::generatePD2IVariableParameter(TR::TreeTop* treeTop, TR::Node* callNode, bool isPD2i, bool isByteBuffer)
   {
   TR::Node* precisionNode = callNode->getChild(2);

   if (comp()->getOption(TR_DisableVariablePrecisionDAA) ||
         !performTransformation(comp(), "O^O TR_DataAccessAccelerator: [DAA] Generating variable %s for node %p \n", isPD2i ? "PD2I" : "PD2L", callNode))
      {
      TR::DebugCounter::incStaticDebugCounter(comp(),
                                              TR::DebugCounter::debugCounterName(comp(),
                                                                                 "DAA/rejected/%s",
                                                                                 isPD2i ? "var-pd2i" : "var-pd2l"));
      return false;
      }

   TR::DebugCounter::incStaticDebugCounter(comp(),
                                           TR::DebugCounter::debugCounterName(comp(),
                                                                              "DAA/inlined/%s",
                                                                              isPD2i ? "var-pd2i" : "var-pd2l"));
   // We will be creating a precision diamond for the fast / slow path and eliminating the original call.
   // Because we are about to split the CFG we must store the original parameters of the call into temp slots
   // as we will be duplicating the call node in the precision diamond. We cannot duplicate the parameters
   // because tree duplication breaks commoning, and thus we want to avoid a situation where a commoned reference
   // to a newarray node gets duplicated and uncommoned.
   callNode = restructureVariablePrecisionCallNode(treeTop, callNode);

   // Duplicate two trees for the precision diamond
   TR::Node* slowNode = callNode->duplicateTree();
   TR::Node* fastNode = callNode->duplicateTree();

   // Create the corresponding treetops
   TR::TreeTop* slowTT = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, slowNode));
   TR::TreeTop* fastTT = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, fastNode));

   // We mark the slow path with a flag to prevent this optimization to recurse on the slow path
   slowNode->setDAAVariableSlowCall(true);

   // Create the precision test diamond
   createPrecisionDiamond(comp(), treeTop, fastTT, slowTT, isPD2i, 1, precisionNode);

   // Fix up any references to the original call
   if (callNode->getReferenceCount() > 0)
      {
      // Create a temp variable to store the result of the call
      TR::SymbolReference* temp = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(),
                                                                          callNode->getDataType());

      TR::TreeTop* slowStore = TR::TreeTop::create(comp(), TR::Node::createStore(temp, slowNode));
      TR::TreeTop* fastStore = TR::TreeTop::create(comp(), TR::Node::createStore(temp, fastNode));

      slowStore->join(slowTT->getNextTreeTop());
      fastStore->join(fastTT->getNextTreeTop());

      slowTT->join(slowStore);
      fastTT->join(fastStore);

      // Replacing original call with a load, so remove all children
      callNode->removeAllChildren();

      // Update the op code to the correct type and make it reference the temp variable
      TR::Node::recreate(callNode, comp()->il.opCodeForDirectLoad(callNode->getDataType()));
      callNode->setSymbolReference(temp);
      }

   // Create BCDCHK node
   TR::SymbolReference*  bcdChkSymRef = fastNode->getSymbolReference();
   TR::Node* pdAddressNode = constructAddressNode(fastNode, fastNode->getChild(0), fastNode->getChild(1));
   TR::Node* bcdchkNode = TR::Node::createWithSymRef(TR::BCDCHK, 2, 2, fastNode, pdAddressNode, bcdChkSymRef);
   fastTT->setNode(bcdchkNode);

   // TreeTop replaced by BCDCHK, so we lose 1 reference
   fastNode->decReferenceCount();

   return true;
   }

bool TR_DataAccessAccelerator::generatePD2I(TR::TreeTop* treeTop, TR::Node* callNode, bool isPD2i, bool isByteBuffer)
   {
   // Check if the current call node is the slow part of a previous variable precision DAA optimization
   if (callNode->isDAAVariableSlowCall())
      {
      return false;
      }

   TR_ASSERT(!IS_VARIABLE_PD2I(callNode), "Variable PD2I should not be handled here.");

   return generatePD2IConstantParameter(treeTop, callNode, isPD2i, isByteBuffer);
   }

bool TR_DataAccessAccelerator::genArithmeticIntrinsic(TR::TreeTop* treeTop, TR::Node* callNode, TR::ILOpCodes opCode)
   {
   if (!isChildConst(callNode, 2) || !isChildConst(callNode, 5) ||
           !isChildConst(callNode, 8) || !isChildConst(callNode, 9))
      {
      return printInliningStatus(false, callNode, "Child (2|5|8|9) is not constant");
      }

   TR_ASSERT(callNode->getNumChildren() == 10, "Expecting BCD arithmetics call with 10 children.");

   TR::Node* resultNode = callNode->getChild(0);
   TR::Node* resOffsetNode = callNode->getChild(1);
   TR::Node* resPrecNode = callNode->getChild(2);
   TR::Node* input1Node = callNode->getChild(3);
   TR::Node* offset1Node = callNode->getChild(4);
   TR::Node* prec1Node = callNode->getChild(5);
   TR::Node* input2Node = callNode->getChild(6);
   TR::Node* offset2Node = callNode->getChild(7);
   TR::Node* prec2Node = callNode->getChild(8);
   TR::Node* overflowNode = callNode->getChild(9);

   int precision1      = prec1Node->getInt();
   int precision2      = prec2Node->getInt();
   int precisionResult = resPrecNode->getInt();
   int isCheckOverflow = overflowNode->getInt();

   if (precision1 < 1 || precision1 > 15 ||
           precision2 < 1 || precision2> 15 ||
           precisionResult < 1 || precisionResult > 15)
      {
      return printInliningStatus(false, callNode, "Invalid precisions. Valid precisions are in range [1, 15]");
      }

   if (performTransformation(comp(), "O^O TR_DataAccessAccelerator: genArithmetics call: %p inlined, with opcode:%d \n", callNode, opCode))
      {
      //create loading
      // loading The first operand
      TR::Node * arrayAddressNodeA = constructAddressNode(callNode, input1Node, offset1Node);
      TR::SymbolReference * symRef1 = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::PackedDecimal, arrayAddressNodeA, 8, fe());
      symRef1->setOffset(0);

      TR::Node * pdloadA = TR::Node::create(TR::pdloadi, 1, arrayAddressNodeA);
      pdloadA->setSymbolReference(symRef1);
      pdloadA->setDecimalPrecision(precision1);

      //load the second operand
      TR::Node * arrayAddressNodeB = constructAddressNode(callNode, input2Node, offset2Node);
      TR::SymbolReference * symRef2 = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::PackedDecimal, arrayAddressNodeB, 8, fe());
      symRef2->setOffset(0);

      TR::Node * pdloadB = TR::Node::create(TR::pdloadi, 1, arrayAddressNodeB);
      pdloadB->setSymbolReference(symRef2);
      pdloadB->setDecimalPrecision(precision2);

      // create actual arithmetic operation.
      TR::Node * operationNode = TR::Node::create(opCode, 2, pdloadA, pdloadB);

      switch(opCode)
         {
         case TR::pdadd:
         case TR::pdsub:
            operationNode->setDecimalPrecision(((precision1 > precision2) ? precision1 : precision2) + 1);
            break;
         case TR::pdmul:
            operationNode->setDecimalPrecision(precision1 + precision2); //TODO: check +1. +1 because pdshlOverflow will do the overflow check
            break;
         case TR::pddiv:
            operationNode->setDecimalPrecision(precision1);
            break;
         case TR::pdrem:
            operationNode->setDecimalPrecision((precision1 < precision2) ? precision1 : precision2);
            break;
         default:
            TR_ASSERT(0, "Unsupported DAA arithmetics opCode");
            break;
         }

      /*
       *  Resulting tree
       *
       * BCDCHK
       *    pdshlOverflow
       *        operationNode
       *             pdLoadA
       *             pdLoadB
       *        iconst 0
       *    arrayAddressNode
       *    call-param-1
       *    ...
       *    call-param-9
       *
       * pdstorei
       *    pdStoreAddressNode
       *    => pdshlOverflow
       *
       * Create separate address nodes for BCDCHK and pdstorei. See generateI2PD() for an explanation to this.
      */
      TR::Node* outOfLineCopyBackAddr  = constructAddressNode(callNode, resultNode, resOffsetNode);
      TR::Node* pdStoreAddressNode     = constructAddressNode(callNode, resultNode, resOffsetNode);

      TR::ILOpCodes op = comp()->il.opCodeForIndirectStore(TR::PackedDecimal);
      TR::SymbolReference * symRefPdstore = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::PackedDecimal, outOfLineCopyBackAddr, 8, fe());
      TR::Symbol * symStore =  TR::Symbol::createShadow(comp()->trHeapMemory(), TR::PackedDecimal, TR::DataType::getSizeFromBCDPrecision(TR::PackedDecimal, resPrecNode->getInt()));
      symStore->setArrayShadowSymbol();
      symRefPdstore->setSymbol(symStore);

      TR::Node * pdshlNode = TR::Node::create(TR::pdshlOverflow, 2, operationNode,
                                              TR::Node::create(callNode, TR::iconst, 0, 0));
      pdshlNode->setDecimalPrecision(resPrecNode->getInt());
      TR::Node * pdstore = TR::Node::create(op, 2, pdStoreAddressNode, pdshlNode);

      TR::SymbolReference* bcdChkSymRef = callNode->getSymbolReference();
      TR::Node * bcdchk = TR::Node::createWithSymRef(TR::BCDCHK, 12, 12,
                                                     pdshlNode, outOfLineCopyBackAddr,
                                                     callNode->getChild(0), callNode->getChild(1),
                                                     callNode->getChild(2), callNode->getChild(3),
                                                     callNode->getChild(4), callNode->getChild(5),
                                                     callNode->getChild(6), callNode->getChild(7),
                                                     callNode->getChild(8), callNode->getChild(9),
                                                     bcdChkSymRef);

      // Set inlined site index to make sure AOT TR_RelocationRecordConstantPool::computeNewConstantPool() API can
      // correctly compute a new CP to relocate DAA OOL calls.
      bcdchk->setInlinedSiteIndex(callNode->getInlinedSiteIndex());

      pdstore->setSymbolReference(symRefPdstore);
      pdstore->setDecimalPrecision(resPrecNode->getInt());

      TR::TreeTop * ttPdstore = TR::TreeTop::create(comp(), pdstore);

      // Join treetops:
      TR::TreeTop * nextTT = treeTop->getNextTreeTop();
      TR::TreeTop * prevTT = treeTop->getPrevTreeTop();
      treeTop->setNode(bcdchk);
      prevTT->join(treeTop);
      treeTop->join(ttPdstore);
      ttPdstore->join(nextTT);

      callNode->recursivelyDecReferenceCount();

      return printInliningStatus(true, callNode);
      }
   return false;
   }

bool TR_DataAccessAccelerator::genShiftRightIntrinsic(TR::TreeTop* treeTop, TR::Node* callNode)
   {
   TR::Node * dstNode = callNode->getChild(0);
   TR::Node * dstOffsetNode = callNode->getChild(1);
   TR::Node * dstPrecNode = callNode->getChild(2);

   TR::Node * srcNode = callNode->getChild(3);
   TR::Node * srcOffsetNode = callNode->getChild(4);
   TR::Node * srcPrecNode = callNode->getChild(5);

   TR::Node * shiftNode = callNode->getChild(6);
   TR::Node * roundNode = callNode->getChild(7);
   TR::Node * overflowNode = callNode->getChild(8);

   int srcPrec = srcPrecNode->getInt();
   int dstPrec = dstPrecNode->getInt();

   int shiftAmount = shiftNode->getInt();
   int isRound   = roundNode->getInt();
   int isCheckOverflow = overflowNode->getInt();
   char* failMsg = NULL;

   if (!isChildConst(callNode, 2) || !isChildConst(callNode, 5) ||
           !isChildConst(callNode, 7) || !isChildConst(callNode, 8))
      failMsg = "Child (2|5|7|8) is not constant";
   else if (srcPrec < 1)
      failMsg = "Invalid precision. Source precision can not be less than 1";
   else if (dstPrec < 1)
      failMsg = "Invalid precision. Destination precision can not be less than 1";
   else if (srcPrec > 15)
      failMsg = "Invalid precision. Source precision can not be greater than 15";
   else if (dstPrec > 15)
      failMsg = "Invalid precision. Destination precision can not be greater than 15";
   else if (dstPrec < srcPrec - shiftAmount)
      failMsg = "Invalid shift amount. Precision is too low to contain shifted Packed Decimal";

   if (!performTransformation(comp(), "O^O TR_DataAccessAccelerator: genShiftRight call: %p inlined.\n", callNode) && !failMsg)
      failMsg = "Not allowed";

   if (failMsg)
      {
      TR::DebugCounter::incStaticDebugCounter(comp(),
                                               TR::DebugCounter::debugCounterName(comp(),
                                                                                  "DAA/rejected/shr"));
      return printInliningStatus(false, callNode, failMsg);
      }

   TR::DebugCounter::incStaticDebugCounter(comp(),
                                           TR::DebugCounter::debugCounterName(comp(),
                                                                              "DAA/inlined/shr"));

   //gen source pdload
   TR::Node * arrayAddressNode = constructAddressNode(callNode, srcNode, srcOffsetNode);

   TR::SymbolReference * symRef = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::PackedDecimal, arrayAddressNode, 8, fe());
   symRef->setOffset(0);

   //gen pdshr:
   TR::Node * roundValueNode = TR::Node::create(callNode,  TR::iconst, 0, isRound ? 5 : 0);
   TR::Node * outOfLineCopyBackAddr = constructAddressNode(callNode, dstNode, dstOffsetNode);
   TR::Node * pdStoreAddressNode    = constructAddressNode(callNode, dstNode, dstOffsetNode);

   TR::Node * pdload = TR::Node::create(TR::pdloadi, 1, arrayAddressNode);
   pdload->setSymbolReference(symRef);
   pdload->setDecimalPrecision(srcPrec);

   TR::Node * pdshrNode = TR::Node::create(TR::pdshr, 3, pdload, shiftNode, roundValueNode);
   pdshrNode->setDecimalPrecision(dstPrec);

   TR::SymbolReference* bcdChkSymRef = callNode->getSymbolReference();
   TR::Node* bcdchkNode = TR::Node::createWithSymRef(TR::BCDCHK, 11, 11,
                                           pdshrNode, outOfLineCopyBackAddr,
                                           callNode->getChild(0), callNode->getChild(1),
                                           callNode->getChild(2), callNode->getChild(3),
                                           callNode->getChild(4), callNode->getChild(5),
                                           callNode->getChild(6), callNode->getChild(7),
                                           callNode->getChild(8),
                                           bcdChkSymRef);

   // Set inlined site index to make sure AOT TR_RelocationRecordConstantPool::computeNewConstantPool() API can
   // correctly compute a new CP to relocate DAA OOL calls.
   bcdchkNode->setInlinedSiteIndex(callNode->getInlinedSiteIndex());

   TR::ILOpCodes op = comp()->il.opCodeForIndirectStore(TR::PackedDecimal);
   TR::SymbolReference * symRefPdstore = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::PackedDecimal, outOfLineCopyBackAddr, 8, fe());
   TR::Symbol * symStore =  TR::Symbol::createShadow(comp()->trHeapMemory(), TR::PackedDecimal, TR::DataType::getSizeFromBCDPrecision(TR::PackedDecimal, dstPrec));
   symStore->setArrayShadowSymbol();
   symRefPdstore->setSymbol(symStore);

   TR::Node * pdstore = TR::Node::create(op, 2, pdStoreAddressNode, pdshrNode);

   pdstore->setSymbolReference(symRefPdstore);
   pdstore->setDecimalPrecision(dstPrec);

   TR::TreeTop * pdstoreNodeTT = TR::TreeTop::create(comp(), pdstore);

   //link them together:
   TR::TreeTop * prevTT = treeTop->getPrevTreeTop();
   TR::TreeTop * nextTT = treeTop->getNextTreeTop();

   prevTT->join(treeTop);
   treeTop->setNode(bcdchkNode);
   treeTop->join(pdstoreNodeTT);
   pdstoreNodeTT->join(nextTT);

   callNode->recursivelyDecReferenceCount();
   return printInliningStatus(true, callNode);
   }

bool TR_DataAccessAccelerator::genShiftLeftIntrinsic(TR::TreeTop* treeTop, TR::Node* callNode)
   {


   TR::Node * dstNode = callNode->getChild(0);
   TR::Node * dstOffsetNode = callNode->getChild(1);
   TR::Node * dstPrecNode = callNode->getChild(2);

   TR::Node * srcNode = callNode->getChild(3);
   TR::Node * srcOffsetNode = callNode->getChild(4);
   TR::Node * srcPrecNode = callNode->getChild(5);

   TR::Node * shiftNode = callNode->getChild(6);

   int srcPrec = srcPrecNode->getInt();
   int dstPrec = dstPrecNode->getInt();
   int shiftAmount = shiftNode->getInt();
   char* failMsg = NULL;

   if (!isChildConst(callNode, 2) || !isChildConst(callNode, 5) ||
           !isChildConst(callNode, 6) || !isChildConst(callNode, 7))
      failMsg = "Child (2|5|6|7) is not constant";
   else if (srcPrec < 1)
      failMsg = "Invalid precision. Source precision can not be less than 1";
   else if (dstPrec < 1)
      failMsg = "Invalid precision. Destination precision can not be less than 1";
   else if (srcPrec > 15)
      failMsg = "Invalid precision. Source precision can not be greater than 15";
   else if (dstPrec > 15)
      failMsg = "Invalid precision. Destination precision can not be greater than 15";
   else if (shiftAmount < 0)
      failMsg = "Invalid shift amount. Shift amount can not be less than 0";

   if (!performTransformation(comp(), "O^O TR_DataAccessAccelerator: genShiftLeft call: %p inlined.\n", callNode) && !failMsg)
      failMsg = "Not allowed";

   if (failMsg)
      {
      TR::DebugCounter::incStaticDebugCounter(comp(),
                                               TR::DebugCounter::debugCounterName(comp(),
                                                                                  "DAA/rejected/shl"));
      return printInliningStatus(false, callNode, failMsg);
      }

   TR::DebugCounter::incStaticDebugCounter(comp(),
                                           TR::DebugCounter::debugCounterName(comp(),
                                                                              "DAA/inlined/shl"));

   TR::Node* srcAddrNode           = constructAddressNode(callNode, srcNode, srcOffsetNode);
   TR::Node* outOfLineCopyBackAddr = constructAddressNode(callNode, dstNode, dstOffsetNode);
   TR::Node* pdStoreAddrNode       = constructAddressNode(callNode, dstNode, dstOffsetNode);

   //pdload:
   TR::Node * pdload = TR::Node::create(TR::pdloadi, 1, srcAddrNode);
   TR::SymbolReference * symRef = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::PackedDecimal, srcAddrNode, 8, fe());
   symRef->setOffset(0);
   pdload->setSymbolReference(symRef);
   pdload->setDecimalPrecision(srcPrec);

   // Always use BCDCHK node for exception handling (invalid digits/sign).
   TR::Node * pdshlNode = TR::Node::create(TR::pdshlOverflow, 2, pdload, shiftNode);
   pdshlNode->setDecimalPrecision(dstPrec);

   TR::SymbolReference* bcdChkSymRef = callNode->getSymbolReference();
   TR::Node* bcdchkNode = TR::Node::createWithSymRef(TR::BCDCHK, 10, 10,
                                       pdshlNode, outOfLineCopyBackAddr,
                                       callNode->getChild(0), callNode->getChild(1),
                                       callNode->getChild(2), callNode->getChild(3),
                                       callNode->getChild(4), callNode->getChild(5),
                                       callNode->getChild(6), callNode->getChild(7),
                                       bcdChkSymRef);

   // Set inlined site index to make sure AOT TR_RelocationRecordConstantPool::computeNewConstantPool() API can
   // correctly compute a new CP to relocate DAA OOL calls.
   bcdchkNode->setInlinedSiteIndex(callNode->getInlinedSiteIndex());

   //following pdstore
   TR::ILOpCodes op = comp()->il.opCodeForIndirectStore(TR::PackedDecimal);
   TR::SymbolReference * symRefPdstore = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::PackedDecimal, outOfLineCopyBackAddr, 8, fe());
   TR::Symbol * symStore =  TR::Symbol::createShadow(comp()->trHeapMemory(), TR::PackedDecimal, TR::DataType::getSizeFromBCDPrecision(TR::PackedDecimal, dstPrec));
   symStore->setArrayShadowSymbol();
   symRefPdstore->setSymbol(symStore);

   TR::Node * pdstore = TR::Node::create(op, 2, pdStoreAddrNode, pdshlNode);
   pdstore->setSymbolReference(symRefPdstore);
   pdstore->setDecimalPrecision(dstPrec);

   //gen treeTop tops
   TR::TreeTop * nextTT      = treeTop->getNextTreeTop();
   TR::TreeTop * prevTT      = treeTop->getPrevTreeTop();
   TR::TreeTop * pdstoreTT   = TR::TreeTop::create(comp(), pdstore);

   prevTT->join(treeTop);
   treeTop->setNode(bcdchkNode);
   treeTop->join(pdstoreTT);
   pdstoreTT->join(nextTT);

   callNode->recursivelyDecReferenceCount();
   return printInliningStatus(true, callNode);
   }

bool TR_DataAccessAccelerator::generateUD2PD(TR::TreeTop* treeTop, TR::Node* callNode, bool isUD2PD)
   {
   TR::Node * decimalNode = callNode->getChild(0);
   TR::Node * decimalOffsetNode = callNode->getChild(1);
   TR::Node * pdNode = callNode->getChild(2);
   TR::Node * pdOffsetNode = callNode->getChild(3);
   TR::Node * precNode = callNode->getChild(4);
   TR::Node * typeNode = callNode->getChild(5);

   //first, check decimalType
   int type = typeNode->getInt();
   int prec = precNode->getInt();
   char* failMsg = NULL;

   if (!isChildConst(callNode, 4) || !isChildConst(callNode, 5))
      failMsg = "Child (4|5) is not constant";
   else if (isUD2PD && type != 5 && type != 6 && type != 7)
      failMsg = "Invalid decimal type. Supported types are (5|6|7)";
   else if (!isUD2PD && (type < 1 || type > 4))
      failMsg = "Invalid decimal type. Supported types are (1|2|3|4)";
   else if (prec < 1 || prec > 31)
      failMsg = "Invalid precision. Valid precision is in range [1, 31]";

   if (failMsg)
      {
      TR::DebugCounter::incStaticDebugCounter(comp(),
                                              TR::DebugCounter::debugCounterName(comp(),
                                                                                 "DAA/rejected/ud2pd"));

      return printInliningStatus(false, callNode, failMsg);
      }

   if (performTransformation(comp(), "O^O TR_DataAccessAccelerator: generate UD2PD/ED2PD call: %p inlined.\n", callNode))
      {
      TR::DebugCounter::incStaticDebugCounter(comp(),
                                              TR::DebugCounter::debugCounterName(comp(),
                                                                                 "DAA/inlined/ud2pd"));

      TR::ILOpCodes loadOp;
      TR::DataType dt = TR::DataTypes::NoType; //unicode data type, as it could be unsigned decimal, sign trailing or sign leading.

      switch (type)
         {
         case 1:
            loadOp = TR::zdloadi;
            dt = TR::ZonedDecimal;
            break;
         case 2:
            loadOp = TR::zdsleLoadi;
            dt = TR::ZonedDecimalSignLeadingEmbedded;
            break;
         case 3:
            loadOp = TR::zdstsLoadi;
            dt = TR::ZonedDecimalSignTrailingSeparate;
            break;
         case 4:
            loadOp = TR::zdslsLoadi;
            dt = TR::ZonedDecimalSignLeadingSeparate;
            break;
         case 5:
            loadOp = TR::udLoadi;
            dt = TR::UnicodeDecimal;
            break;
         case 6:
            loadOp = TR::udslLoadi;
            dt = TR::UnicodeDecimalSignLeading;
            break;
         case 7:
            loadOp = TR::udstLoadi;
            dt = TR::UnicodeDecimalSignTrailing;
            break;
         default:
            TR_ASSERT(false, "illegal decimalType.\n");
         }

      //create decimalload
      TR::Node * decimalAddressNode;
      int offset = decimalOffsetNode->getInt();
      TR::Node * twoConstNode;
      TR::Node * multipliedOffsetNode;
      TR::Node * totalOffsetNode;
      TR::Node * headerConstNode;
      if (TR::Compiler->target.is64Bit())
         {
         headerConstNode = TR::Node::create(callNode, TR::lconst, 0, 0);
         headerConstNode->setLongInt(TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
         twoConstNode = TR::Node::create(callNode, TR::lconst, 0, 0);
         twoConstNode->setLongInt(isUD2PD ? 2 : 1);
         multipliedOffsetNode = TR::Node::create(TR::lmul, 2,
                                                TR::Node::create(TR::i2l, 1, decimalOffsetNode), twoConstNode);
         totalOffsetNode = TR::Node::create(TR::ladd, 2, headerConstNode, multipliedOffsetNode);
         decimalAddressNode = TR::Node::create(TR::aladd, 2, decimalNode, totalOffsetNode);
         }
      else
         {
         headerConstNode = TR::Node::create(callNode, TR::iconst, 0,
                                           TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
         twoConstNode = TR::Node::create(callNode, TR::iconst, 0, isUD2PD ? 2 : 1);
         multipliedOffsetNode = TR::Node::create(TR::imul, 2, decimalOffsetNode, twoConstNode);
         totalOffsetNode = TR::Node::create(TR::iadd, 2, headerConstNode, multipliedOffsetNode);
         decimalAddressNode = TR::Node::create(TR::aiadd, 2, decimalNode, totalOffsetNode);
         }

      decimalAddressNode->setIsInternalPointer(true);
      TR::SymbolReference * symRef = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(dt, decimalAddressNode, 8, fe());
      symRef->setOffset(0);

      TR::Node * decimalload = TR::Node::create(loadOp, 1, decimalAddressNode);
      decimalload->setSymbolReference(symRef);
      decimalload->setDecimalPrecision(prec);

      //create PDaddress
      TR::Node * pdAddressNode = constructAddressNode(callNode, pdNode, pdOffsetNode);

      int elementSize = isUD2PD ? TR::DataType::getUnicodeElementSize() : TR::DataType::getZonedElementSize();

      //bound values
      int pdPrecSize = TR::DataType::getSizeFromBCDPrecision(TR::PackedDecimal, prec) - 1;
      int decimalPrecSize = (TR::DataType::getSizeFromBCDPrecision(dt, prec) / elementSize) - 1;
      TR::Node * pdBndvalue = TR::Node::create(TR::iadd, 2,
                                              pdOffsetNode,
                                              TR::Node::create(callNode, TR::iconst, 0, pdPrecSize));
      TR::Node * decimalBndvalue = TR::Node::create(TR::iadd, 2,
                                              decimalOffsetNode,
                                              TR::Node::create(callNode, TR::iconst, 0, decimalPrecSize)); //size of unicode is 2 bytes

      //create ud2pd
      TR::ILOpCodes op = TR::BadILOp;
      TR::ILOpCodes interOp = TR::BadILOp;
      switch (type)
         {
         case 1:
            op = TR::zd2pd;
            break;
         case 2:
            interOp = TR::zdsle2zd;
            op = TR::zd2pd;
            break;
         case 3:
            interOp = TR::zdsts2zd;
            op = TR::zd2pd;
            break;
         case 4:
            interOp = TR::zdsls2zd;
            op = TR::zd2pd;
            break;
         case 5:
            op = TR::ud2pd;
            break;
         case 6:
            op = TR::udsl2pd;
            break;
         case 7:
            op = TR::udst2pd;
            break;
         default:
            TR_ASSERT(false, "illegal decimalType.\n");
         }

      TR::Node * decimal2pdNode = NULL;
      if (isUD2PD || type == 1)
         {
         //for converting zd to pd (here type == 1), dont need the additional intermediate conversion
         decimal2pdNode = TR::Node::create(op, 1, decimalload);
         }
      else //ED2PD, needs intermediate conversion
         {
         TR::Node * decimal2zdNode = TR::Node::create(interOp, 1, decimalload);
         decimal2zdNode->setDecimalPrecision(prec);
         decimal2pdNode = TR::Node::create(op, 1, decimal2zdNode);
         }
         decimal2pdNode->setDecimalPrecision(prec);

      //create pdstore
      TR::Node * pdstoreNode = TR::Node::create(TR::pdstorei, 2, pdAddressNode, decimal2pdNode);
      TR::SymbolReference * symRefStore = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::PackedDecimal, pdAddressNode, 8, fe());
      TR::Symbol * symStore =  TR::Symbol::createShadow(comp()->trHeapMemory(), TR::PackedDecimal, TR::DataType::getSizeFromBCDPrecision(TR::PackedDecimal, prec));
      symRefStore->setSymbol(symStore);

      pdstoreNode->setSymbolReference(symRefStore);
      pdstoreNode->setDecimalPrecision(prec);

      //set up bndchks, and null chks
      TR::Node * pdPassThroughNode = TR::Node::create(TR::PassThrough, 1, pdNode);
      TR::Node * decimalPassThroughNode = TR::Node::create(TR::PassThrough, 1, decimalNode);

      TR::Node * pdArrayLengthNode = TR::Node::create(TR::arraylength, 1, pdNode);
      TR::Node * decimalArrayLengthNode = TR::Node::create(TR::arraylength, 1, decimalNode);

      TR::Node * pdNullChkNode = TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, pdPassThroughNode, getSymRefTab()->findOrCreateRuntimeHelper(TR_nullCheck, false, true, true));
      TR::Node * decimalNullChkNode = TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, decimalPassThroughNode, getSymRefTab()->findOrCreateRuntimeHelper(TR_nullCheck, false, true, true));

      TR::Node * pdBndChk = TR::Node::createWithSymRef(TR::BNDCHK, 2, 2, pdArrayLengthNode, pdOffsetNode, getSymRefTab()->findOrCreateRuntimeHelper(TR_arrayBoundsCheck, false, true, true));
      TR::Node * pdBndChk2 =TR::Node::createWithSymRef(TR::BNDCHK, 2, 2, pdArrayLengthNode, pdBndvalue,   getSymRefTab()->findOrCreateRuntimeHelper(TR_arrayBoundsCheck, false, true, true));
      TR::Node * decimalBndChk = TR::Node::createWithSymRef(TR::BNDCHK, 2, 2, decimalArrayLengthNode, decimalOffsetNode,
                                                getSymRefTab()->findOrCreateRuntimeHelper(TR_arrayBoundsCheck, false, true, true));
      TR::Node * decimalBndChk2 =TR::Node::createWithSymRef(TR::BNDCHK, 2, 2, decimalArrayLengthNode, decimalBndvalue,
                                                getSymRefTab()->findOrCreateRuntimeHelper(TR_arrayBoundsCheck, false, true, true));

      //gen tree tops
      TR::TreeTop * pdNullChktt = TR::TreeTop::create(comp(), pdNullChkNode);
      TR::TreeTop * decimalNullChktt = TR::TreeTop::create(comp(), decimalNullChkNode);

      TR::TreeTop * pdBndChktt1 = TR::TreeTop::create(comp(), pdBndChk);
      TR::TreeTop * pdBndChktt2 = TR::TreeTop::create(comp(), pdBndChk2);
      TR::TreeTop * decimalBndChktt1 = TR::TreeTop::create(comp(), decimalBndChk );
      TR::TreeTop * decimalBndChktt2 = TR::TreeTop::create(comp(), decimalBndChk2);

      TR::TreeTop * ttPdstore = TR::TreeTop::create(comp(), pdstoreNode);


      //link together
      TR::TreeTop * nextTT = treeTop->getNextTreeTop();
      TR::TreeTop * prevTT = treeTop->getPrevTreeTop();

      prevTT->join(decimalNullChktt);
      decimalNullChktt->join(pdNullChktt);
      pdNullChktt->join(decimalBndChktt1);
      decimalBndChktt1->join(decimalBndChktt2);
      decimalBndChktt2->join(pdBndChktt1);
      pdBndChktt1->join(pdBndChktt2);
      pdBndChktt2->join(ttPdstore);
      ttPdstore->join(nextTT);

      callNode->recursivelyDecReferenceCount();
      return true;
      }

   return false;
   }

bool TR_DataAccessAccelerator::generatePD2UD(TR::TreeTop* treeTop, TR::Node* callNode, bool isPD2UD)
   {
   TR::Node * pdNode = callNode->getChild(0);
   TR::Node * pdOffsetNode = callNode->getChild(1);
   TR::Node * decimalNode = callNode->getChild(2);
   TR::Node * decimalOffsetNode = callNode->getChild(3);
   TR::Node * precNode = callNode->getChild(4);
   TR::Node * typeNode = callNode->getChild(5);

   //first, check decimalType
   int type = typeNode->getInt();
   char* failMsg = NULL;
   int prec = precNode->getInt();

   if (!isChildConst(callNode, 4) || !isChildConst(callNode, 5))
      failMsg = "Child (4|5) is not constant";
   else if (isPD2UD && type != 5 && type != 6 && type != 7)
      failMsg = "Invalid decimal type. Supported types are (5|6|7)";
   else if (!isPD2UD && (type < 1 || type > 4)) //PD2ED
      failMsg = "Invalid decimal type. Supported types are (1|2|3|4)";
   else if (prec < 1 || prec > 31)
      failMsg = "Invalid precision. Valid precision is in range [1, 31]";

   if (failMsg)
      {
      TR::DebugCounter::incStaticDebugCounter(comp(),
                                              TR::DebugCounter::debugCounterName(comp(),
                                                                                 "DAA/rejected/%s",
                                                                                 isPD2UD ? "pd2ud" : "pd2ed"));

      return printInliningStatus(false, callNode, failMsg);
      }

   if (performTransformation(comp(), "O^O TR_DataAccessAccelerator: generate PD2UD/PD2ED call: %p inlined.\n", callNode))
      {
      TR::DebugCounter::incStaticDebugCounter(comp(),
                                              TR::DebugCounter::debugCounterName(comp(),
                                                                                 "DAA/inlined/%s",
                                                                                 isPD2UD ? "pd2ud" : "pd2ed"));

      //set up pdload:
      TR::Node * arrayAddressNode = constructAddressNode(callNode, pdNode, pdOffsetNode);

      int size = TR::DataType::getSizeFromBCDPrecision(TR::PackedDecimal, prec) - 1;
      TR::SymbolReference * symRef = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::PackedDecimal, arrayAddressNode, 8, fe());
      symRef->setOffset(0);

      TR::Node * pdload = TR::Node::create(TR::pdloadi, 1, arrayAddressNode);
      pdload->setSymbolReference(symRef);
      pdload->setDecimalPrecision(prec);

      //set up decimal arrayAddressNode
      TR::Node * decimalAddressNode;
         {
         TR::Node * twoConstNode;
         TR::Node * multipliedOffsetNode;
         TR::Node * totalOffsetNode;
         TR::Node * headerConstNode;
         if (TR::Compiler->target.is64Bit())
            {
            headerConstNode = TR::Node::create(callNode, TR::lconst, 0, 0);
            headerConstNode->setLongInt(TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
            twoConstNode = TR::Node::create(callNode, TR::lconst, 0, 0);
            twoConstNode->setLongInt(isPD2UD ? 2 : 1);
            multipliedOffsetNode = TR::Node::create(TR::lmul, 2,
                                                   TR::Node::create(TR::i2l, 1, decimalOffsetNode), twoConstNode);
            totalOffsetNode = TR::Node::create(TR::ladd, 2, headerConstNode, multipliedOffsetNode);
            decimalAddressNode = TR::Node::create(TR::aladd, 2, decimalNode, totalOffsetNode);
            }
         else
            {
            headerConstNode = TR::Node::create(callNode, TR::iconst, 0,
                                              TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
            twoConstNode = TR::Node::create(callNode, TR::iconst, 0, isPD2UD ? 2 : 1);
            multipliedOffsetNode = TR::Node::create(TR::imul, 2, decimalOffsetNode, twoConstNode);
            totalOffsetNode = TR::Node::create(TR::iadd, 2, headerConstNode, multipliedOffsetNode);
            decimalAddressNode = TR::Node::create(TR::aiadd, 2, decimalNode, totalOffsetNode);
            }
         decimalAddressNode->setIsInternalPointer(true);
         }

      //set up pd2decimal node
      TR::ILOpCodes op = TR::BadILOp;
      TR::ILOpCodes storeOp = TR::BadILOp;
      TR::ILOpCodes interOp = TR::BadILOp;
      TR::DataType dt = TR::NoType;
      switch (type)
         {
         case 1:
            op = TR::pd2zd;
            storeOp = TR::zdstorei;
            dt = TR::ZonedDecimal;
            break;
         case 2:
            op = TR::zd2zdsle;
            interOp = TR::pd2zd;
            storeOp = TR::zdsleStorei;
            dt = TR::ZonedDecimalSignLeadingEmbedded;
            break;
         case 3:
            op = TR::zd2zdsts;
            interOp = TR::pd2zd;
            storeOp = TR::zdstsStorei;
            dt = TR::ZonedDecimalSignTrailingSeparate;
            break;
         case 4:
            op = TR::zd2zdsls;
            interOp = TR::pd2zd;
            storeOp = TR::zdslsStorei;
            dt = TR::ZonedDecimalSignLeadingSeparate;
            break;
         case 5:
            op = TR::pd2ud;
            interOp = TR::pd2ud;
            storeOp = TR::udStorei;
            dt = TR::UnicodeDecimal;
            break;
         case 6:
            op = TR::pd2udsl;
            interOp = TR::pd2ud;
            storeOp = TR::udslStorei;
            dt = TR::UnicodeDecimalSignLeading;
            break;
         case 7:
            op = TR::pd2udst;
            interOp = TR::pd2ud;
            storeOp = TR::udstStorei;
            dt = TR::UnicodeDecimalSignTrailing;
            break;
         default:
         TR_ASSERT(false, "unsupported decimalType.\n");
         }

      TR::Node * pd2decimalNode;
      if (isPD2UD || type == 1)
         {
         pd2decimalNode = TR::Node::create(op, 1, pdload);
         }
      else //ED2PD
         {
         TR::Node * toZDNode = TR::Node::create(interOp, 1, pdload);
         toZDNode->setDecimalPrecision(precNode->getInt());
         pd2decimalNode = TR::Node::create(op, 1, toZDNode);
         }
      pd2decimalNode->setDecimalPrecision(precNode->getInt());

      //set up decimalStore node
      TR::SymbolReference * symRefDecimalStore = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(dt, decimalAddressNode, 8, fe());
      TR::Symbol * symStore =  TR::Symbol::createShadow(comp()->trHeapMemory(), dt, TR::DataType::getSizeFromBCDPrecision(dt, prec));
      symStore->setArrayShadowSymbol();
      symRefDecimalStore->setSymbol(symStore);

      TR::Node * decimalStore = TR::Node::create(storeOp, 2, decimalAddressNode, pd2decimalNode);
      decimalStore->setSymbolReference(symRefDecimalStore);
      decimalStore->setDecimalPrecision(precNode->getInt());

      //set up bndchks, and null chks
      TR::Node * pdPassThroughNode = TR::Node::create(TR::PassThrough, 1, pdNode);
      TR::Node * decimalPassThroughNode = TR::Node::create(TR::PassThrough, 1, decimalNode);
      int elementSize = isPD2UD ? TR::DataType::getUnicodeElementSize() : TR::DataType::getZonedElementSize();
      int pdPrecSize = TR::DataType::getSizeFromBCDPrecision(TR::PackedDecimal, prec) - 1;
      int decimalPrecSize = (TR::DataType::getSizeFromBCDPrecision(dt, prec) / elementSize) - 1;
      TR::Node * pdBndvalue = TR::Node::create(TR::iadd, 2, pdOffsetNode, TR::Node::create(callNode, TR::iconst, 0, pdPrecSize));
      TR::Node * decimalBndvalue = TR::Node::create(TR::iadd, 2, decimalOffsetNode, TR::Node::create(callNode, TR::iconst, 0, decimalPrecSize)); //size of unicode is 2 bytes

      TR::Node * pdArrayLengthNode = TR::Node::create(TR::arraylength, 1, pdNode);
      TR::Node * decimalArrayLengthNode = TR::Node::create(TR::arraylength, 1, decimalNode);

      TR::Node * pdNullChkNode = TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, pdPassThroughNode, getSymRefTab()->findOrCreateRuntimeHelper(TR_nullCheck, false, true, true));
      TR::Node * decimalNullChkNode = TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, decimalPassThroughNode, getSymRefTab()->findOrCreateRuntimeHelper(TR_nullCheck, false, true, true));

      TR::Node * pdBndChk = TR::Node::createWithSymRef(TR::BNDCHK, 2, 2, pdArrayLengthNode, pdOffsetNode, getSymRefTab()->findOrCreateRuntimeHelper(TR_arrayBoundsCheck, false, true, true));
      TR::Node * pdBndChk2 =TR::Node::createWithSymRef(TR::BNDCHK, 2, 2, pdArrayLengthNode, pdBndvalue, getSymRefTab()->findOrCreateRuntimeHelper(TR_arrayBoundsCheck, false, true, true));
      TR::Node * decimalBndChk = TR::Node::createWithSymRef(TR::BNDCHK, 2, 2, decimalArrayLengthNode, decimalOffsetNode, getSymRefTab()->findOrCreateRuntimeHelper(TR_arrayBoundsCheck, false, true, true));
      TR::Node * decimalBndChk2 =TR::Node::createWithSymRef(TR::BNDCHK, 2, 2, decimalArrayLengthNode, decimalBndvalue, getSymRefTab()->findOrCreateRuntimeHelper(TR_arrayBoundsCheck, false, true, true));

      //gen tree tops
      TR::TreeTop * nextTT = treeTop->getNextTreeTop();
      TR::TreeTop * prevTT = treeTop->getPrevTreeTop();

      TR::TreeTop * pdNullChktt = TR::TreeTop::create(comp(), pdNullChkNode);
      TR::TreeTop * decimalNullChktt = TR::TreeTop::create(comp(), decimalNullChkNode);

      TR::TreeTop * pdBndChktt1 = TR::TreeTop::create(comp(), pdBndChk);
      TR::TreeTop * pdBndChktt2 = TR::TreeTop::create(comp(), pdBndChk2);
      TR::TreeTop * decimalBndChktt1 = TR::TreeTop::create(comp(), decimalBndChk);
      TR::TreeTop * decimalBndChktt2 = TR::TreeTop::create(comp(), decimalBndChk2);

      TR::TreeTop * decimalStoreTT    = TR::TreeTop::create(comp(), decimalStore);

      prevTT->join(pdNullChktt);
      pdNullChktt->join(decimalNullChktt);
      decimalNullChktt->join(pdBndChktt1);
      pdBndChktt1->join(pdBndChktt2);
      pdBndChktt2->join(decimalBndChktt1);
      decimalBndChktt1->join(decimalBndChktt2);
      decimalBndChktt2->join(decimalStoreTT);
      decimalStoreTT->join(nextTT);

      callNode->recursivelyDecReferenceCount();

      return true;
      }

   return false;
   }

void TR_DataAccessAccelerator::insertByteArrayNULLCHK(TR::TreeTop* callTreeTop, TR::Node* callNode, TR::Node* byteArrayNode)
   {
   TR::Compilation* comp = OMR::Optimization::comp();

   callTreeTop->insertBefore(TR::TreeTop::create(comp, TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, TR::Node::create(TR::PassThrough, 1, byteArrayNode), comp->getSymRefTab()->findOrCreateNullCheckSymbolRef(callNode->getSymbol()->getResolvedMethodSymbol()))));
   }

void TR_DataAccessAccelerator::insertByteArrayBNDCHK(TR::TreeTop* callTreeTop,  TR::Node* callNode, TR::Node* byteArrayNode, TR::Node* offsetNode, int32_t index)
   {
   TR::Compilation* comp = OMR::Optimization::comp();

   if (index != 0)
      {
      offsetNode = TR::Node::create(TR::iadd, 2, offsetNode, TR::Node::create(callNode, TR::iconst, 0, index));
      }

   TR::Node* arraylengthNode = TR::Node::create(TR::arraylength, 1, byteArrayNode);

   // byte[] is always of type TR::Int8 so set the appropriate stride
   arraylengthNode->setArrayStride(TR::Symbol::convertTypeToSize(TR::Int8));

   callTreeTop->insertBefore(TR::TreeTop::create(comp, TR::Node::createWithSymRef(TR::BNDCHK, 2, 2, arraylengthNode, offsetNode, comp->getSymRefTab()->findOrCreateArrayBoundsCheckSymbolRef(callNode->getSymbol()->getResolvedMethodSymbol()))));
   }

TR::Node* TR_DataAccessAccelerator::createByteArrayElementAddress(TR::TreeTop* callTreeTop, TR::Node* callNode, TR::Node* byteArrayNode, TR::Node* offsetNode)
   {
   TR::CodeGenerator* cg = comp()->cg();

   TR::Node* byteArrayElementAddressNode;

   if (TR::Compiler->target.is64Bit())
      {
      byteArrayElementAddressNode = TR::Node::create(TR::aladd, 2, byteArrayNode, TR::Node::create(TR::ladd, 2, TR::Node::create(callNode, TR::lconst, 0, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()), TR::Node::create(TR::i2l, 1, offsetNode)));
      }
   else
      {
      byteArrayElementAddressNode = TR::Node::create(TR::aiadd, 2, byteArrayNode, TR::Node::create(TR::iadd, 2, TR::Node::create(callNode, TR::iconst, 0, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()), offsetNode));
      }

   // This node is pointing to an array element so we must mark it as such
   byteArrayElementAddressNode->setIsInternalPointer(true);

   return byteArrayElementAddressNode;
   }
