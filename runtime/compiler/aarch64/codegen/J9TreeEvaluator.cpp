/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include "codegen/ARM64Instruction.hpp"
#include "codegen/ARM64JNILinkage.hpp"
#include "codegen/ARM64PrivateLinkage.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/J9ARM64Snippet.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/OMRDataTypes_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"

/*
 * J9 ARM64 specific tree evaluator table overrides
 */
extern void TEMPORARY_initJ9ARM64TreeEvaluatorTable(TR::CodeGenerator *cg)
   {
   TR_TreeEvaluatorFunctionPointer *tet = cg->getTreeEvaluatorTable();

   // TODO:ARM64: Enable when Implemented: tet[TR::awrtbar] = TR::TreeEvaluator::awrtbarEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::awrtbari] = TR::TreeEvaluator::awrtbariEvaluator;
   tet[TR::monexit] = TR::TreeEvaluator::monexitEvaluator;
   tet[TR::monent] = TR::TreeEvaluator::monentEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::monexitfence] = TR::TreeEvaluator::monexitfenceEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::asynccheck] = TR::TreeEvaluator::asynccheckEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::instanceof] = TR::TreeEvaluator::instanceofEvaluator;
   tet[TR::checkcast] = TR::TreeEvaluator::checkcastEvaluator;
   tet[TR::checkcastAndNULLCHK] = TR::TreeEvaluator::checkcastAndNULLCHKEvaluator;
   tet[TR::New] = TR::TreeEvaluator::newObjectEvaluator;
   tet[TR::variableNew] = TR::TreeEvaluator::newObjectEvaluator;
   tet[TR::newarray] = TR::TreeEvaluator::newArrayEvaluator;
   tet[TR::anewarray] = TR::TreeEvaluator::anewArrayEvaluator;
   tet[TR::variableNewArray] = TR::TreeEvaluator::anewArrayEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::multianewarray] = TR::TreeEvaluator::multianewArrayEvaluator;
   tet[TR::arraylength] = TR::TreeEvaluator::arraylengthEvaluator;
   tet[TR::ResolveCHK] = TR::TreeEvaluator::resolveCHKEvaluator;
   tet[TR::DIVCHK] = TR::TreeEvaluator::DIVCHKEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::BNDCHK] = TR::TreeEvaluator::BNDCHKEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::ArrayCopyBNDCHK] = TR::TreeEvaluator::ArrayCopyBNDCHKEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::BNDCHKwithSpineCHK] = TR::TreeEvaluator::BNDCHKwithSpineCHKEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::SpineCHK] = TR::TreeEvaluator::BNDCHKwithSpineCHKEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::ArrayStoreCHK] = TR::TreeEvaluator::ArrayStoreCHKEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::ArrayCHK] = TR::TreeEvaluator::ArrayCHKEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::MethodEnterHook] = TR::TreeEvaluator::conditionalHelperEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::MethodExitHook] = TR::TreeEvaluator::conditionalHelperEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::allocationFence] = TR::TreeEvaluator::flushEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::loadFence] = TR::TreeEvaluator::flushEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::storeFence] = TR::TreeEvaluator::flushEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::fullFence] = TR::TreeEvaluator::flushEvaluator;
   }

void VMgenerateCatchBlockBBStartPrologue(TR::Node *node, TR::Instruction *fenceInstruction, TR::CodeGenerator *cg)
   {
   TR_UNIMPLEMENTED();
   }

void
J9::ARM64::TreeEvaluator::generateTestAndReportFieldWatchInstructions(TR::CodeGenerator *cg, TR::Node *node, TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister, TR::Register *valueReg, TR::Register *dataSnippetRegister)
   {
   TR_ASSERT_FATAL(false, "This helper implements platform specific code for Fieldwatch, which is currently not supported on ARM64 platforms.\n");
   }

void
J9::ARM64::TreeEvaluator::generateFillInDataBlockSequenceForUnresolvedField(TR::CodeGenerator *cg, TR::Node *node, TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister, TR::Register *dataSnippetRegister)
   {
   TR_ASSERT_FATAL(false, "This helper implements platform specific code for Fieldwatch, which is currently not supported on ARM64 platforms.\n");
   }

TR::Register *
J9::ARM64::TreeEvaluator::DIVCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *divisor = node->getFirstChild()->getSecondChild();
   bool is64Bit = node->getFirstChild()->getType().isInt64();
   bool isConstDivisor = divisor->getOpCode().isLoadConst();

   if (!isConstDivisor || (!is64Bit && divisor->getInt() == 0) || (is64Bit && divisor->getLongInt() == 0))
      {
      TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
      cg->addSnippet(new (cg->trHeapMemory()) TR::ARM64HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference()));

      if (isConstDivisor)
         {
         // No explicit check required
         generateLabelInstruction(cg, TR::InstOpCode::b, node, snippetLabel);
         }
      else
         {
         TR::Register *divisorReg = cg->evaluate(divisor);
         TR::InstOpCode::Mnemonic compareOp = is64Bit ? TR::InstOpCode::cbzx : TR::InstOpCode::cbzw;
         generateCompareBranchInstruction(cg, compareOp, node, divisorReg, snippetLabel, NULL);
         }
      }

   cg->evaluate(node->getFirstChild());
   cg->decReferenceCount(node->getFirstChild());
   return NULL;
   }

TR::Register *
J9::ARM64::TreeEvaluator::monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::call);
   TR::Register *targetRegister = directCallEvaluator(node, cg);
   TR::Node::recreate(node, opCode);
   return targetRegister;
   }

TR::Register *
J9::ARM64::TreeEvaluator::checkcastAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return checkcastEvaluator(node, cg);
   }

TR::Register *
J9::ARM64::TreeEvaluator::checkcastEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::call);
   TR::Register *targetRegister = directCallEvaluator(node, cg);
   TR::Node::recreate(node, opCode);

   return targetRegister;
   }

TR::Register *
J9::ARM64::TreeEvaluator::newObjectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::acall);
   TR::Register *targetRegister = directCallEvaluator(node, cg);
   TR::Node::recreate(node, opCode);
   return targetRegister;
   }

TR::Register *
J9::ARM64::TreeEvaluator::newArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::acall);
   TR::Register *targetRegister = directCallEvaluator(node, cg);
   TR::Node::recreate(node, opCode);
   return targetRegister;
   }

TR::Register *
J9::ARM64::TreeEvaluator::anewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::acall);
   TR::Register *targetRegister = directCallEvaluator(node, cg);
   TR::Node::recreate(node, opCode);
   return targetRegister;
   }

TR::Register *
J9::ARM64::TreeEvaluator::monentEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::call);
   TR::Register *targetRegister = directCallEvaluator(node, cg);
   TR::Node::recreate(node, opCode);
   return targetRegister;
   }

TR::Register *
J9::ARM64::TreeEvaluator::arraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   // ldrw R, [B + contiguousSize]        
   // cmp R, 0                            
   // cselw R, [B + discontiguousSize]    
   //
   TR::Register *objectReg = cg->evaluate(node->getFirstChild());
   TR::Register *lengthReg = cg->allocateRegister();
   TR::Register *discontiguousLengthReg = cg->allocateRegister();

   TR::MemoryReference *contiguousArraySizeMR = new (cg->trHeapMemory()) TR::MemoryReference(objectReg, fej9->getOffsetOfContiguousArraySizeField(), cg);
   TR::MemoryReference *discontiguousArraySizeMR = new (cg->trHeapMemory()) TR::MemoryReference(objectReg, fej9->getOffsetOfDiscontiguousArraySizeField(), cg);

   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrw, node, lengthReg, contiguousArraySizeMR);
   generateCompareImmInstruction(cg, node, lengthReg, 0);
   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrw, node, discontiguousLengthReg, discontiguousArraySizeMR);
   generateCondTrg1Src2Instruction(cg, TR::InstOpCode::cselw, node, lengthReg, lengthReg, discontiguousLengthReg, TR::CC_NE);

   cg->stopUsingRegister(discontiguousLengthReg);
   cg->decReferenceCount(node->getFirstChild());
   node->setRegister(lengthReg);

   return lengthReg;
   }
