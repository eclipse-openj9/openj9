/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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
#include "codegen/ARM64OutOfLineCodeSection.hpp"
#include "codegen/ARM64PrivateLinkage.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/ARM64Instruction.hpp"
#include "codegen/J9ARM64Snippet.hpp"
#include "codegen/OMRCodeGenerator.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "il/DataTypes.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/OMRDataTypes_inlines.hpp"

/*
 * J9 ARM64 specific tree evaluator table overrides
 */
extern void TEMPORARY_initJ9ARM64TreeEvaluatorTable(TR::CodeGenerator *cg)
   {
   TR_TreeEvaluatorFunctionPointer *tet = cg->getTreeEvaluatorTable();

   tet[TR::awrtbar] = TR::TreeEvaluator::awrtbarEvaluator;
   tet[TR::awrtbari] = TR::TreeEvaluator::awrtbariEvaluator;
   tet[TR::monexit] = TR::TreeEvaluator::monexitEvaluator;
   tet[TR::monent] = TR::TreeEvaluator::monentEvaluator;
   tet[TR::monexitfence] = TR::TreeEvaluator::monexitfenceEvaluator;
   tet[TR::asynccheck] = TR::TreeEvaluator::asynccheckEvaluator;
   tet[TR::instanceof] = TR::TreeEvaluator::instanceofEvaluator;
   tet[TR::checkcast] = TR::TreeEvaluator::checkcastEvaluator;
   tet[TR::checkcastAndNULLCHK] = TR::TreeEvaluator::checkcastAndNULLCHKEvaluator;
   tet[TR::New] = TR::TreeEvaluator::newObjectEvaluator;
   tet[TR::variableNew] = TR::TreeEvaluator::newObjectEvaluator;
   tet[TR::newarray] = TR::TreeEvaluator::newArrayEvaluator;
   tet[TR::anewarray] = TR::TreeEvaluator::anewArrayEvaluator;
   tet[TR::variableNewArray] = TR::TreeEvaluator::anewArrayEvaluator;
   tet[TR::multianewarray] = TR::TreeEvaluator::multianewArrayEvaluator;
   tet[TR::arraylength] = TR::TreeEvaluator::arraylengthEvaluator;
   tet[TR::ZEROCHK] = TR::TreeEvaluator::ZEROCHKEvaluator;
   tet[TR::ResolveCHK] = TR::TreeEvaluator::resolveCHKEvaluator;
   tet[TR::DIVCHK] = TR::TreeEvaluator::DIVCHKEvaluator;
   tet[TR::BNDCHK] = TR::TreeEvaluator::BNDCHKEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::ArrayCopyBNDCHK] = TR::TreeEvaluator::ArrayCopyBNDCHKEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::BNDCHKwithSpineCHK] = TR::TreeEvaluator::BNDCHKwithSpineCHKEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::SpineCHK] = TR::TreeEvaluator::BNDCHKwithSpineCHKEvaluator;
   tet[TR::ArrayStoreCHK] = TR::TreeEvaluator::ArrayStoreCHKEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::ArrayCHK] = TR::TreeEvaluator::ArrayCHKEvaluator;
   tet[TR::MethodEnterHook] = TR::TreeEvaluator::conditionalHelperEvaluator;
   tet[TR::MethodExitHook] = TR::TreeEvaluator::conditionalHelperEvaluator;
   tet[TR::allocationFence] = TR::TreeEvaluator::flushEvaluator;
   tet[TR::loadFence] = TR::TreeEvaluator::flushEvaluator;
   tet[TR::storeFence] = TR::TreeEvaluator::flushEvaluator;
   tet[TR::fullFence] = TR::TreeEvaluator::flushEvaluator;
   tet[TR::frem] = TR::TreeEvaluator::fremEvaluator;
   tet[TR::drem] = TR::TreeEvaluator::dremEvaluator;
   tet[TR::NULLCHK] = TR::TreeEvaluator::NULLCHKEvaluator;
   tet[TR::ResolveAndNULLCHK] = TR::TreeEvaluator::resolveAndNULLCHKEvaluator;
   }

void VMgenerateCatchBlockBBStartPrologue(TR::Node *node, TR::Instruction *fenceInstruction, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

   TR::Block *block = node->getBlock();

   if (fej9->shouldPerformEDO(block, comp))
      {
      TR_UNIMPLEMENTED();
      }
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

static void wrtbarEvaluator(TR::Node *node, TR::Register *srcReg, TR::Register *dstReg, bool srcNonNull, bool needDeps, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   // assuming gcMode == gc_modron_wrtbar_always;
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   TR::RegisterDependencyConditions *conditions = NULL;

   if (needDeps)
      {
      conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 2, cg->trMemory());
      TR::addDependency(conditions, dstReg,  TR::RealRegister::x0, TR_GPR, cg);
      TR::addDependency(conditions, srcReg,  TR::RealRegister::x1, TR_GPR, cg);
      }

   TR::SymbolReference *wbRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef();
   // nullcheck store - skip write barrier if null being written in
   if (!srcNonNull)
      {
      generateCompareBranchInstruction(cg, TR::InstOpCode::cbzx, node, srcReg, doneLabel, NULL);
      }
   generateImmSymInstruction(cg, TR::InstOpCode::bl, node, (uintptr_t)wbRef->getMethodAddress(),
                                        new (cg->trHeapMemory()) TR::RegisterDependencyConditions((uint8_t)0, 0, cg->trMemory()),
                                        wbRef, NULL);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions, NULL);

   cg->machine()->setLinkRegisterKilled(true);
   }

TR::Register *
J9::ARM64::TreeEvaluator::conditionalHelperEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *testNode = node->getFirstChild();
   TR::Node *callNode = node->getSecondChild();
   TR::Node *firstChild = testNode->getFirstChild();
   TR::Node *secondChild = testNode->getSecondChild();
   TR::Register *jumpReg = cg->evaluate(firstChild);
   TR::Register *valReg = NULL;
   int32_t i, numArgs = callNode->getNumChildren();
   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(3, 3, cg->trMemory());

   TR_ASSERT(numArgs <= 2, "Unexpected number of arguments for helper.");

   // Helper arguments are in reversed order of the private linkage
   // Argument registers are not needed to be split since the helper will
   // preserve all of them.
   int32_t iArgIndex = 0, fArgIndex = 0;
   TR::Linkage *linkage = cg->createLinkage(TR_Private);
   for (i = numArgs - 1; i >= 0; i--)
      {
      TR::Register *argReg = cg->evaluate(callNode->getChild(i));
      TR::addDependency(conditions, argReg, (argReg->getKind() == TR_GPR) ? // Didn't consider Long here
            linkage->getProperties().getIntegerArgumentRegister(iArgIndex++) : linkage->getProperties().getFloatArgumentRegister(fArgIndex++), argReg->getKind(), cg);
      }

   TR::addDependency(conditions, jumpReg, TR::RealRegister::x8, TR_GPR, cg);
   bool is64Bit = node->getSecondChild()->getType().isInt64();
   int64_t value = is64Bit ? secondChild->getLongInt() : secondChild->getInt();
   if (secondChild->getOpCode().isLoadConst() && constantIsUnsignedImm12(value))
      {
      generateCompareImmInstruction(cg, testNode, jumpReg, value);
      }
   else
      {
      valReg = cg->evaluate(secondChild);
      generateCompareInstruction(cg, testNode, jumpReg, valReg);
      }

   TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
   TR::Snippet *snippet = new (cg->trHeapMemory()) TR::ARM64HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference());
   cg->addSnippet(snippet);
   TR::ARM64ConditionCode cc = (testNode->getOpCodeValue() == TR::icmpeq) ? TR::CC_EQ : TR::CC_NE;
   TR::Instruction *gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, snippetLabel, cc, conditions);
   gcPoint->ARM64NeedsGCMap(cg, 0xFFFFFFFF);
   snippet->gcMap().setGCRegisterMask(0xffffffff);

   for (i = numArgs - 1; i >= 0; i--)
      cg->decReferenceCount(callNode->getChild(i));
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->decReferenceCount(testNode);
   cg->decReferenceCount(callNode);
   return NULL;
   }

TR::Register *
J9::ARM64::TreeEvaluator::awrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *valueReg = cg->evaluate(firstChild);

   TR::Register *destinationRegister = cg->evaluate(node->getSecondChild());

   TR::Register *sourceRegister;
   bool killSource = false;
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());

   if (node->getSymbolReference()->getSymbol()->isShadow() && node->getSymbolReference()->getSymbol()->isOrdered() && cg->comp()->target().isSMP())
      {
      needSync = true;
      }
   if (firstChild->getReferenceCount() > 1 && firstChild->getRegister() != NULL)
      {
      if (!firstChild->getRegister()->containsInternalPointer())
         sourceRegister = cg->allocateCollectedReferenceRegister();
      else
         {
         sourceRegister = cg->allocateRegister();
         sourceRegister->setPinningArrayPointer(firstChild->getRegister()->getPinningArrayPointer());
         sourceRegister->setContainsInternalPointer();
         }
      generateMovInstruction(cg, node, sourceRegister, firstChild->getRegister());
      killSource = true;
      }
   else
      sourceRegister = valueReg;

   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, TR::Compiler->om.sizeofReferenceAddress(), cg);

   if (needSync)
      generateSynchronizationInstruction(cg, TR::InstOpCode::dmb, node, 0xE);

   generateMemSrc1Instruction(cg, TR::InstOpCode::strimmx, node, tempMR, sourceRegister, NULL);
   wrtbarEvaluator(node, sourceRegister, destinationRegister, firstChild->isNonNull(), true, cg);

   if (killSource)
      cg->stopUsingRegister(sourceRegister);

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   tempMR->decNodeReferenceCounts(cg);

   return NULL;
   }

TR::Register *
J9::ARM64::TreeEvaluator::awrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   TR::Register *destinationRegister = cg->evaluate(node->getChild(2));
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *sourceRegister;
   bool killSource = false;
   bool usingCompressedPointers = false;
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());

   if (node->getSymbolReference()->getSymbol()->isShadow() && node->getSymbolReference()->getSymbol()->isOrdered() && cg->comp()->target().isSMP())
      {
      needSync = true;
      }

   if (comp->useCompressedPointers() && (node->getSymbolReference()->getSymbol()->getDataType() == TR::Address) && (node->getSecondChild()->getDataType() != TR::Address))
      {
      // pattern match the sequence
      //     awrtbari f    awrtbari f         <- node
      //       aload O       aload O
      //     value           l2i
      //                       lshr         <- translatedNode
      //                         lsub
      //                           a2l
      //                             value   <- secondChild
      //                           lconst HB
      //                         iconst shftKonst
      //
      // -or- if the field is known to be null or usingLowMemHeap
      // awrtbari f
      //    aload O
      //    l2i
      //      a2l
      //        value  <- secondChild
      //
      TR::Node *translatedNode = secondChild;
      if (translatedNode->getOpCode().isConversion())
         translatedNode = translatedNode->getFirstChild();
      if (translatedNode->getOpCode().isRightShift()) // optional
         translatedNode = translatedNode->getFirstChild();

      bool usingLowMemHeap = false;
      if (TR::Compiler->vm.heapBaseAddress() == 0 || secondChild->isNull())
         usingLowMemHeap = true;

      if (translatedNode->getOpCode().isSub() || usingLowMemHeap)
         usingCompressedPointers = true;

      if (usingCompressedPointers)
         {
         while (secondChild->getNumChildren() && secondChild->getOpCodeValue() != TR::a2l)
            secondChild = secondChild->getFirstChild();
         if (secondChild->getNumChildren())
            secondChild = secondChild->getFirstChild();
         }
      }

   if (secondChild->getReferenceCount() > 1 && secondChild->getRegister() != NULL)
      {
      if (!secondChild->getRegister()->containsInternalPointer())
         sourceRegister = cg->allocateCollectedReferenceRegister();
      else
         {
         sourceRegister = cg->allocateRegister();
         sourceRegister->setPinningArrayPointer(secondChild->getRegister()->getPinningArrayPointer());
         sourceRegister->setContainsInternalPointer();
         }
      generateMovInstruction(cg, node, sourceRegister, secondChild->getRegister());
      killSource = true;
      }
   else
      {
      sourceRegister = cg->evaluate(secondChild);
      }

   TR::InstOpCode::Mnemonic storeOp = usingCompressedPointers ? TR::InstOpCode::strimmw : TR::InstOpCode::strimmx;
   TR::Register *translatedSrcReg = usingCompressedPointers ? cg->evaluate(node->getSecondChild()) : sourceRegister;
   int32_t sizeofMR = usingCompressedPointers ? TR::Compiler->om.sizeofReferenceField() : TR::Compiler->om.sizeofReferenceAddress();

   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, sizeofMR, cg);

   if (needSync)
      generateSynchronizationInstruction(cg, TR::InstOpCode::dmb, node, 0xE);

   generateMemSrc1Instruction(cg, storeOp, node, tempMR, translatedSrcReg);

   wrtbarEvaluator(node, sourceRegister, destinationRegister, secondChild->isNonNull(), true, cg);

   if (killSource)
      cg->stopUsingRegister(sourceRegister);

   cg->decReferenceCount(node->getSecondChild());
   cg->decReferenceCount(node->getChild(2));
   tempMR->decNodeReferenceCounts(cg);

   if (comp->useCompressedPointers())
      node->setStoreAlreadyEvaluated(true);

   return NULL;
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
      TR::Instruction *gcPoint;
      TR::Snippet *snippet = new (cg->trHeapMemory()) TR::ARM64HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference());
      cg->addSnippet(snippet);

      if (isConstDivisor)
         {
         // No explicit check required
         gcPoint = generateLabelInstruction(cg, TR::InstOpCode::b, node, snippetLabel);
         }
      else
         {
         TR::Register *divisorReg = cg->evaluate(divisor);
         TR::InstOpCode::Mnemonic compareOp = is64Bit ? TR::InstOpCode::cbzx : TR::InstOpCode::cbzw;
         gcPoint = generateCompareBranchInstruction(cg, compareOp, node, divisorReg, snippetLabel, NULL);
         }
      gcPoint->ARM64NeedsGCMap(cg, 0xffffffff);
      snippet->gcMap().setGCRegisterMask(0xffffffff);
      }

   cg->evaluate(node->getFirstChild());
   cg->decReferenceCount(node->getFirstChild());
   return NULL;
   }

TR::Register *
J9::ARM64::TreeEvaluator::monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = TR::comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   int32_t lwOffset = fej9->getByteOffsetToLockword(cg->getMonClass(node));
   TR::InstOpCode::Mnemonic op;

   if (comp->getOption(TR_OptimizeForSpace) ||
       comp->getOption(TR_FullSpeedDebug) ||
       comp->getOption(TR_DisableInlineMonEnt) ||
       lwOffset <= 0)
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::call);
      TR::Register *targetRegister = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      return targetRegister;
      }

   TR::Node *objNode = node->getFirstChild();
   TR::Register *objReg = cg->evaluate(objNode);
   TR::Register *dataReg = cg->allocateRegister();
   TR::Register *addrReg = cg->allocateRegister();
   TR::Register *tempReg = cg->allocateRegister();
   TR::Register *metaReg = cg->getMethodMetaDataRegister();

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(4, 4, cg->trMemory());
   TR::addDependency(deps, objReg, TR::RealRegister::x0, TR_GPR, cg);
   TR::addDependency(deps, dataReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(deps, addrReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(deps, tempReg, TR::RealRegister::NoReg, TR_GPR, cg);

   TR::LabelSymbol *callLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *decLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addimmx, node, addrReg, objReg, lwOffset);
   op = fej9->generateCompressedLockWord() ? TR::InstOpCode::ldrimmw : TR::InstOpCode::ldrimmx;
   generateTrg1MemInstruction(cg, op, node, dataReg, new (cg->trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg));
   generateCompareInstruction(cg, node, dataReg, metaReg, true);

   generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, decLabel, TR::CC_NE);

   generateTrg1ImmInstruction(cg, TR::InstOpCode::movzx, node, tempReg, 0);
   generateSynchronizationInstruction(cg, TR::InstOpCode::dmb, node, 0xF); // dmb SY
   op = fej9->generateCompressedLockWord() ? TR::InstOpCode::strimmw : TR::InstOpCode::strimmx;
   generateMemSrc1Instruction(cg, op, node, new (cg->trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg), tempReg);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, deps);

   TR::Snippet *snippet = new (cg->trHeapMemory()) TR::ARM64MonitorExitSnippet(cg, node, decLabel, callLabel, doneLabel);
   cg->addSnippet(snippet);
   doneLabel->setEndInternalControlFlow();

   cg->decReferenceCount(objNode);
   cg->machine()->setLinkRegisterKilled(true);
   return NULL;
   }

TR::Register *
J9::ARM64::TreeEvaluator::asynccheckEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The child contains an inline test. If it succeeds, the helper is called.
   // The address of the helper is contained as a long in this node.
   //
   TR::Node *testNode = node->getFirstChild();
   TR::Node *firstChild = testNode->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Node *secondChild = testNode->getSecondChild();
   TR::Register *src2Reg = cg->evaluate(secondChild);

   TR_ASSERT(testNode->getOpCodeValue() == TR::lcmpeq, "asynccheck bad format");

   TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   TR::SymbolReference *asynccheckHelper = node->getSymbolReference();
   TR::Snippet *snippet = new (cg->trHeapMemory()) TR::ARM64HelperCallSnippet(cg, node, snippetLabel, asynccheckHelper, doneLabel);
   cg->addSnippet(snippet);

   // ToDo:
   // Optimize this using "cmp (immediate)" instead of "cmp (register)" when possible
   generateCompareInstruction(cg, node, src1Reg, src2Reg, true); // 64-bit compare

   TR::Instruction *gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, snippetLabel, TR::CC_EQ);
   gcPoint->ARM64NeedsGCMap(cg, 0xFFFFFFFF);
   snippet->gcMap().setGCRegisterMask(0xffffffff);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);

   // ARM64HelperCallSnippet generates "bl" instruction
   cg->machine()->setLinkRegisterKilled(true);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->decReferenceCount(testNode);
   return NULL;
   }

TR::Register *
J9::ARM64::TreeEvaluator::instanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // Call helper
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::icall);
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
J9::ARM64::TreeEvaluator::flushEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes op = node->getOpCodeValue();

   if (op == TR::allocationFence)
      {
      if (!node->canOmitSync())
         {
         // Data synchronization barrier -- 0xF to turn on option for both reads and writes
         generateSynchronizationInstruction(cg, TR::InstOpCode::dsb, node, 0xF);
         }
      }
   else
      {
      uint32_t imm;
      if (op == TR::loadFence)
         {
         // 0xD to turn on option for reads
         imm = 0xD;
         }
      else if (op == TR::storeFence)
         {
         // 0xE to turn on option for writes
         imm = 0xE;
         }
      else
         {
         // 0xF to turn on option for both reads and writes
         imm = 0xF;
         }
      generateSynchronizationInstruction(cg, TR::InstOpCode::dsb, node, imm);
      }

   return NULL;
   }

TR::Register *
J9::ARM64::TreeEvaluator::multianewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::acall);
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
   TR::Compilation *comp = TR::comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   int32_t lwOffset = fej9->getByteOffsetToLockword(cg->getMonClass(node));
   TR::InstOpCode::Mnemonic op;

   if (comp->getOption(TR_OptimizeForSpace) ||
       comp->getOption(TR_FullSpeedDebug) ||
       comp->getOption(TR_DisableInlineMonEnt) ||
       lwOffset <= 0)
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::call);
      TR::Register *targetRegister = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      return targetRegister;
      }

   TR::Node *objNode = node->getFirstChild();
   TR::Register *objReg = cg->evaluate(objNode);
   TR::Register *dataReg = cg->allocateRegister();
   TR::Register *addrReg = cg->allocateRegister();
   TR::Register *tempReg = cg->allocateRegister();
   TR::Register *metaReg = cg->getMethodMetaDataRegister();

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(4, 4, cg->trMemory());
   TR::addDependency(deps, objReg, TR::RealRegister::x0, TR_GPR, cg);
   TR::addDependency(deps, dataReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(deps, addrReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(deps, tempReg, TR::RealRegister::NoReg, TR_GPR, cg);

   TR::LabelSymbol *callLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *incLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *loopLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addimmx, node, addrReg, objReg, lwOffset); // ldxr/stxr instructions does not take immediate offset
   generateLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel);

   op = fej9->generateCompressedLockWord() ? TR::InstOpCode::ldxrw : TR::InstOpCode::ldxrx;
   generateTrg1MemInstruction(cg, op, node, dataReg, new (cg->trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg));
   generateCompareBranchInstruction(cg, TR::InstOpCode::cbnzx, node, dataReg, incLabel, NULL);

   op = fej9->generateCompressedLockWord() ? TR::InstOpCode::stxrw : TR::InstOpCode::stxrx;
   generateTrg1MemSrc1Instruction(cg, op, node, tempReg, new (cg->trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg), metaReg);
   generateCompareBranchInstruction(cg, TR::InstOpCode::cbnzx, node, tempReg, loopLabel, NULL);

   generateSynchronizationInstruction(cg, TR::InstOpCode::dmb, node, 0xF); // dmb SY

   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, deps);

   TR::Snippet *snippet = new (cg->trHeapMemory()) TR::ARM64MonitorEnterSnippet(cg, node, incLabel, callLabel, doneLabel);
   cg->addSnippet(snippet);
   doneLabel->setEndInternalControlFlow();

   cg->decReferenceCount(objNode);
   cg->machine()->setLinkRegisterKilled(true);
   return NULL;
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

TR::Register *
J9::ARM64::TreeEvaluator::ZEROCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::LabelSymbol *slowPathLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *restartLabel = generateLabelSymbol(cg);
   slowPathLabel->setStartInternalControlFlow();
   restartLabel->setEndInternalControlFlow();

   // Temporarily hide the first child so it doesn't appear in the outlined call
   //
   node->rotateChildren(node->getNumChildren()-1, 0);
   node->setNumChildren(node->getNumChildren()-1);

   // Outlined instructions for check failure
   // Note: we don't pass the restartLabel in here because we don't want a
   // restart branch.
   //
   TR_ARM64OutOfLineCodeSection *outlinedHelperCall = new (cg->trHeapMemory()) TR_ARM64OutOfLineCodeSection(node, TR::call, NULL, slowPathLabel, restartLabel, cg);
   cg->getARM64OutOfLineCodeSectionList().push_front(outlinedHelperCall);

   // Restore the first child
   //
   node->setNumChildren(node->getNumChildren()+1);
   node->rotateChildren(0, node->getNumChildren()-1);

   // Children other than the first are only for the outlined path; we don't need them here
   //
   for (int32_t i = 1; i < node->getNumChildren(); i++)
      cg->recursivelyDecReferenceCount(node->getChild(i));

   // In-line instructions for the check
   // ToDo: Optimize isBooleanCompare() case
   //
   TR::Node *valueToCheck = node->getFirstChild();
   TR::Register *value = cg->evaluate(valueToCheck);
   generateCompareBranchInstruction(cg, TR::InstOpCode::cbzx, node, value, slowPathLabel);
   cg->decReferenceCount(node->getFirstChild());

   generateLabelInstruction(cg, TR::InstOpCode::label, node, restartLabel);

   return NULL;
   }

TR::Register *
J9::ARM64::TreeEvaluator::BNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *secondChild = node->getSecondChild();
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg;
   TR::Register *src2Reg = NULL;
   uint64_t value;
   TR::LabelSymbol *snippetLabel;
   TR::Instruction *gcPoint;
   bool reversed = false;

   if ((firstChild->getOpCode().isLoadConst())
         && (constantIsUnsignedImm12(firstChild->get64bitIntegralValueAsUnsigned()))
         && (NULL == firstChild->getRegister()))
      {
      src2Reg = cg->evaluate(secondChild);
      reversed = true;
      }
   else
      {
      src1Reg = cg->evaluate(firstChild);

      if ((secondChild->getOpCode().isLoadConst())
            && (NULL == secondChild->getRegister()))
         {
         value = secondChild->get64bitIntegralValueAsUnsigned();
         if (!constantIsUnsignedImm12(value))
            {
            src2Reg = cg->evaluate(secondChild);
            }
         }
      else
         src2Reg = cg->evaluate(secondChild);
      }

   if (reversed)
      {
      generateCompareImmInstruction(cg, node, src2Reg, firstChild->get64bitIntegralValueAsUnsigned());
      }
   else
      {
      if (NULL == src2Reg)
         generateCompareImmInstruction(cg, node, src1Reg, value);
      else
         generateCompareInstruction(cg, node, src1Reg, src2Reg);
      }

   snippetLabel = generateLabelSymbol(cg);
   TR::Snippet *snippet = new (cg->trHeapMemory()) TR::ARM64HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference());
   cg->addSnippet(snippet);

   gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, snippetLabel, (reversed ? TR::CC_CS : TR::CC_LS));

   gcPoint->ARM64NeedsGCMap(cg, 0xFFFFFFFF);
   snippet->gcMap().setGCRegisterMask(0xffffffff);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   secondChild->setIsNonNegative(true);
   return (NULL);
   }

static void VMoutlinedHelperArrayStoreCHKEvaluator(TR::Node *node, TR::Register *srcReg, TR::Register *dstReg, TR::CodeGenerator *cg)
   {
   TR::SymbolReference *arrayStoreChkHelper = node->getSymbolReference();

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 2, cg->trMemory());
   TR::addDependency(deps, dstReg, TR::RealRegister::x0, TR_GPR, cg);
   TR::addDependency(deps, srcReg, TR::RealRegister::x1, TR_GPR, cg);

   TR::Instruction *gcPoint = generateImmSymInstruction(cg, TR::InstOpCode::bl, node,
                                                        (uintptr_t)arrayStoreChkHelper->getMethodAddress(),
                                                        deps, arrayStoreChkHelper, NULL);
   gcPoint->ARM64NeedsGCMap(cg, 0xFFFFFFFF);

   cg->machine()->setLinkRegisterKilled(true);
   }

static void VMoutlinedHelperWrtbarEvaluator(TR::Node *node, TR::Register *srcReg, TR::Register *dstReg, TR::CodeGenerator *cg)
   {
   auto gcMode = TR::Compiler->om.writeBarrierType();
   if (gcMode == gc_modron_wrtbar_none)
      return;

   TR::Compilation *comp = cg->comp();
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   TR::SymbolReference *wbref = comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef(comp->getMethodSymbol());

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 2, cg->trMemory());
   TR::addDependency(deps, dstReg, TR::RealRegister::x0, TR_GPR, cg);
   TR::addDependency(deps, srcReg, TR::RealRegister::x1, TR_GPR, cg);

   generateCompareBranchInstruction(cg, TR::InstOpCode::cbzx, node, srcReg, doneLabel);

   TR::Instruction *gcPoint = generateImmSymInstruction(cg, TR::InstOpCode::bl, node,
                                                        (uintptr_t)wbref->getMethodAddress(),
                                                        new (cg->trHeapMemory()) TR::RegisterDependencyConditions((uint8_t)0, 0, cg->trMemory()),
                                                        wbref, NULL);
   gcPoint->ARM64NeedsGCMap(cg, 0xFFFFFFFF);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, deps);

   cg->machine()->setLinkRegisterKilled(true);
   }

TR::Register *
J9::ARM64::TreeEvaluator::ArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *sourceChild = firstChild->getSecondChild();
   TR::Node *dstNode = firstChild->getThirdChild();

   bool usingCompressedPointers = false;
   if (comp->useCompressedPointers() && firstChild->getOpCode().isIndirect())
      {
      // pattern match the sequence
      // ArrayStoreCHK     ArrayStoreCHK
      //   awrtbari          awrtbari               <- firstChild
      //     aladd             aladd
      //       ...               ...
      //     value             l2i
      //     aload               lshr
      //                           lsub
      //                             a2l
      //                               value        <- sourceChild
      //                             lconst HB
      //                           iconst shftKonst
      //                       aload                <- dstNode
      //
      // -or- if the value is known to be null
      // ArrayStoreCHK
      //    awrtbari
      //      aladd
      //        ...
      //      l2i
      //        a2l
      //          value  <- sourceChild
      //      aload      <- dstNode
      //
      TR::Node *translatedNode = sourceChild;
      if (translatedNode->getOpCode().isConversion())
         translatedNode = translatedNode->getFirstChild();
      if (translatedNode->getOpCode().isRightShift()) // optional
         translatedNode = translatedNode->getFirstChild();

      bool usingLowMemHeap = false;
      if (TR::Compiler->vm.heapBaseAddress() == 0 || sourceChild->isNull())
         usingLowMemHeap = true;

      if ((translatedNode->getOpCode().isSub()) || usingLowMemHeap)
         usingCompressedPointers = true;

      if (usingCompressedPointers)
         {
         while ((sourceChild->getNumChildren() > 0) && (sourceChild->getOpCodeValue() != TR::a2l))
            sourceChild = sourceChild->getFirstChild();
         if (sourceChild->getOpCodeValue() == TR::a2l)
            sourceChild = sourceChild->getFirstChild();
         }
      }

   TR::Register *srcReg;
   TR::Register *dstReg = cg->evaluate(dstNode);
   bool stopUsingSrc = false;
   if (sourceChild->getReferenceCount() > 1 && (srcReg = sourceChild->getRegister()) != NULL)
      {
      TR::Register *tempReg = cg->allocateCollectedReferenceRegister();

      // Source must be an object.
      TR_ASSERT(!srcReg->containsInternalPointer(), "Stored value is an internal pointer");
      generateMovInstruction(cg, node, tempReg, srcReg);
      srcReg = tempReg;
      stopUsingSrc = true;
      }
   else
      {
      srcReg = cg->evaluate(sourceChild);
      }
   if (!sourceChild->isNull())
      {
      VMoutlinedHelperArrayStoreCHKEvaluator(node, srcReg, dstReg, cg);
      }
   TR::InstOpCode::Mnemonic storeOp = usingCompressedPointers ? TR::InstOpCode::strimmw : TR::InstOpCode::strimmx;
   TR::Register *translatedSrcReg = usingCompressedPointers ? cg->evaluate(firstChild->getSecondChild()) : srcReg;
   int32_t sizeofMR = usingCompressedPointers ? TR::Compiler->om.sizeofReferenceField() : TR::Compiler->om.sizeofReferenceAddress();

   TR::MemoryReference *storeMR = new (cg->trHeapMemory()) TR::MemoryReference(firstChild, sizeofMR, cg);
   generateMemSrc1Instruction(cg, storeOp, node, storeMR, translatedSrcReg);

   if (!sourceChild->isNull())
      {
      VMoutlinedHelperWrtbarEvaluator(firstChild, srcReg, dstReg, cg);
      }

   if (comp->useCompressedPointers() && firstChild->getOpCode().isIndirect())
      firstChild->setStoreAlreadyEvaluated(true);

   cg->decReferenceCount(firstChild->getSecondChild());
   cg->decReferenceCount(dstNode);
   storeMR->decNodeReferenceCounts(cg);
   cg->decReferenceCount(firstChild);
   if (stopUsingSrc)
      cg->stopUsingRegister(srcReg);

   return NULL;
   }

static TR::Register *
genCAS(TR::Node *node, TR::CodeGenerator *cg, TR::Register *objReg, TR::Register *offsetReg, TR::Register *oldVReg, TR::Register *newVReg,
      TR::LabelSymbol *doneLabel, TR::Node *objNode, int32_t oldValue, bool oldValueInReg, bool isLong, bool casWithoutSync = false)
   {
   TR_ASSERT_FATAL(false, "CAS generation is currently unsupported.\n");
   }

static TR::Register *
VMinlineCompareAndSwap(TR::Node *node, TR::CodeGenerator *cg, bool isLong)
   {
   TR::Compilation * comp = cg->comp();
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Node *thirdChild = node->getChild(2);
   TR::Node *fourthChild = node->getChild(3);
   TR::Node *fifthChild = node->getChild(4);
   TR::Register *offsetReg = NULL;
   TR::Register *oldVReg = NULL;
   TR::Register *newVReg = NULL;
   TR::Register *resultReg = NULL;
   TR::Register *objReg = cg->evaluate(secondChild);
   TR::RegisterDependencyConditions *conditions = NULL;
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   intptrj_t oldValue = 0;
   bool oldValueInReg = true;
   offsetReg = cg->evaluate(thirdChild);

   // Obtain values to be checked for, and swapped in:
   if (fourthChild->getOpCode().isLoadConst() && fourthChild->getRegister() == NULL)
      {
      if (isLong)
         oldValue = fourthChild->getLongInt();
      else
         oldValue = fourthChild->getInt();
      if (constantIsUnsignedImm12(oldValue))
         oldValueInReg = false;
      }
   if (oldValueInReg)
      oldVReg = cg->evaluate(fourthChild);
   newVReg = cg->evaluate(fifthChild);

   // Determine if synchronization needed:
   bool casWithoutSync = false;
   TR_OpaqueMethodBlock *caller = node->getOwningMethod();
   if (caller)
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
      TR_ResolvedMethod *m = fej9->createResolvedMethod(cg->trMemory(), caller, node->getSymbolReference()->getOwningMethod(comp));
      if ((m->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicInteger_weakCompareAndSet)
            || (m->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicLong_weakCompareAndSet)
            || (m->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicReference_weakCompareAndSet))
         {
         casWithoutSync = true;
         }
      }

   // Compare and swap:
   resultReg = genCAS(node, cg, objReg, offsetReg, oldVReg, newVReg, doneLabel, secondChild, oldValue, oldValueInReg, isLong, casWithoutSync);

   conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(5, 5, cg->trMemory());
   TR::addDependency(conditions, objReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, offsetReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, resultReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, newVReg, TR::RealRegister::NoReg, TR_GPR, cg);
   if (oldValueInReg)
      TR::addDependency(conditions, oldVReg, TR::RealRegister::NoReg, TR_GPR, cg);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);

   cg->recursivelyDecReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->decReferenceCount(thirdChild);
   if (oldValueInReg)
      cg->decReferenceCount(fourthChild);
   else
      cg->recursivelyDecReferenceCount(fourthChild);
   cg->decReferenceCount(fifthChild);
   return resultReg;
   }

static TR::Register *
VMinlineFMA(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL(node->getNumChildren() == 3, "In function VMinlineFMA, the node at address %p should have exactly 3 children, but got %u instead", node, node->getNumChildren());

   TR::DataType type = node->getDataType();
   TR_ASSERT_FATAL(type == TR::Float || type == TR::Double, "In function VMinlineFMA, the node at address %p should be either TR::Float or TR::Double", node);

   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Node *thirdChild = node->getThirdChild();
   TR::Register *src1Register = cg->evaluate(firstChild);
   TR::Register *src2Register = cg->evaluate(secondChild);
   TR::Register *src3Register = cg->evaluate(thirdChild);

   if(type == TR::Float)
      generateTrg1Src3Instruction(cg, TR::InstOpCode::fmadds, node, src1Register, src1Register, src2Register, src3Register);
   else
      generateTrg1Src3Instruction(cg, TR::InstOpCode::fmaddd, node, src1Register, src1Register, src2Register, src3Register);

   node->setRegister(src1Register);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->decReferenceCount(thirdChild);
   return src1Register;
   }

bool
J9::ARM64::CodeGenerator::inlineDirectCall(TR::Node *node, TR::Register *&resultReg)
   {
   TR::CodeGenerator *cg = self();
   TR::MethodSymbol * methodSymbol = node->getSymbol()->getMethodSymbol();

   if (methodSymbol)
      {
      switch (methodSymbol->getRecognizedMethod())
         {
         case TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z:
            {
            // TODO return false here as this method is still WIP
            return false;

            if (!methodSymbol->isNative())
               break;

            if ((node->isUnsafeGetPutCASCallOnNonArray() || !TR::Compiler->om.canGenerateArraylets()) && node->isSafeForCGToFastPathUnsafeCall())
               {
               resultReg = VMinlineCompareAndSwap(node, cg, false);
               return true;
               }
            break;
            }
         case TR::java_lang_Math_fma_D:
         case TR::java_lang_StrictMath_fma_D:
         case TR::java_lang_Math_fma_F:
         case TR::java_lang_StrictMath_fma_F:
            resultReg = VMinlineFMA(node, cg);
            return true;

         default:
            break;
         }
      }

   // Nothing was done
   resultReg = NULL;
   return false;
   }

TR::Instruction *J9::ARM64::TreeEvaluator::generateVFTMaskInstruction(TR::CodeGenerator *cg, TR::Node *node, TR::Register *dstReg, TR::Register *srcReg, TR::Instruction *preced)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   uintptrj_t mask = TR::Compiler->om.maskOfObjectVftField();
   bool isCompressed = TR::Compiler->om.compressObjectReferences();

   if (~mask == 0)
      {
      // no mask instruction required
      return preced;
      }
   else if (~mask == 0xFF)
      {
      TR::InstOpCode::Mnemonic op = isCompressed ? TR::InstOpCode::andimmw : TR::InstOpCode::andimmx;
      uint32_t imm = isCompressed ? 0x617 : 0xE37; // encoding for ~0xFF
      return generateLogicalImmInstruction(cg, op, node, dstReg, srcReg, !isCompressed, imm, preced);
      }
   else
      {
      TR_UNIMPLEMENTED();
      }
   }

TR::Instruction *J9::ARM64::TreeEvaluator::generateVFTMaskInstruction(TR::CodeGenerator *cg, TR::Node *node, TR::Register *reg, TR::Instruction *preced)
   {
   return J9::ARM64::TreeEvaluator::generateVFTMaskInstruction(cg, node, reg, reg, preced);
   }

TR::Register *
J9::ARM64::TreeEvaluator::loadaddrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *resultReg;
   TR::Symbol *sym = node->getSymbol();
   TR::Compilation *comp = cg->comp();
   TR::MemoryReference *mref = new (cg->trHeapMemory()) TR::MemoryReference(node, node->getSymbolReference(), 0, cg);

   if (mref->getUnresolvedSnippet() != NULL)
      {
      resultReg = sym->isLocalObject() ? cg->allocateCollectedReferenceRegister() : cg->allocateRegister();
      if (mref->useIndexedForm())
         {
         TR_ASSERT(false, "Unresolved indexed snippet is not supported");
         }
      else
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::addx, node, resultReg, mref);
         }
      }
   else
      {
      if (mref->useIndexedForm())
         {
         resultReg = sym->isLocalObject() ? cg->allocateCollectedReferenceRegister() : cg->allocateRegister();
         generateTrg1Src2Instruction(cg, TR::InstOpCode::addx, node, resultReg, mref->getBaseRegister(), mref->getIndexRegister());
         }
      else
         {
         int32_t offset = mref->getOffset();
         if (mref->hasDelayedOffset() || offset != 0)
            {
            resultReg = sym->isLocalObject() ? cg->allocateCollectedReferenceRegister() : cg->allocateRegister();
            if (mref->hasDelayedOffset())
               {
               generateTrg1MemInstruction(cg, TR::InstOpCode::addimmx, node, resultReg, mref);
               }
            else
               {
               if (offset >= 0 && constantIsUnsignedImm12(offset))
                  {
                  generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addimmx, node, resultReg, mref->getBaseRegister(), offset);
                  }
               else
                  {
                  loadConstant64(cg, node, offset, resultReg);
                  generateTrg1Src2Instruction(cg, TR::InstOpCode::addx, node, resultReg, mref->getBaseRegister(), resultReg);
                  }
               }
            }
         else
            {
            resultReg = mref->getBaseRegister();
            if (resultReg == cg->getMethodMetaDataRegister())
               {
               resultReg = cg->allocateRegister();
               generateMovInstruction(cg, node, resultReg, mref->getBaseRegister());
               }
            }
         }
      }
   node->setRegister(resultReg);
   mref->decNodeReferenceCounts(cg);
   return resultReg;
   }

TR::Register *J9::ARM64::TreeEvaluator::fremHelper(TR::Node *node, TR::CodeGenerator *cg, bool isSinglePrecision)
   {
   TR::Register *trgReg      = isSinglePrecision ? cg->allocateSinglePrecisionRegister() : cg->allocateRegister(TR_FPR);
   TR::Node     *child1      = node->getFirstChild();
   TR::Node     *child2      = node->getSecondChild();
   TR::Register *source1Reg  = cg->evaluate(child1);
   TR::Register *source2Reg  = cg->evaluate(child2);

   if (!cg->canClobberNodesRegister(child1))
      {
      auto copyReg = isSinglePrecision ? cg->allocateSinglePrecisionRegister() : cg->allocateRegister(TR_FPR);
      generateTrg1Src1Instruction(cg, isSinglePrecision ? TR::InstOpCode::fmovs : TR::InstOpCode::fmovd, node, copyReg, source1Reg);
      source1Reg = copyReg;
      }
   if (!cg->canClobberNodesRegister(child2))
      {
      auto copyReg = isSinglePrecision ? cg->allocateSinglePrecisionRegister() : cg->allocateRegister(TR_FPR);
      generateTrg1Src1Instruction(cg, isSinglePrecision ? TR::InstOpCode::fmovs : TR::InstOpCode::fmovd, node, copyReg, source2Reg);
      source2Reg = copyReg;
      }

   // We call helperCFloatRemainderFloat, thus we need to set appropriate register dependencies.
   // First, count all volatile registers.
   TR::Linkage *linkage = cg->createLinkage(TR_System);
   auto linkageProp = linkage->getProperties();
   int nregs = 0;
   for (int32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastAssignableFPR; i++)
      {
      if ((linkageProp._registerFlags[i] != ARM64_Reserved) && (linkageProp._registerFlags[i] != Preserved))
         {
         nregs++;
         }
      }

   TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(nregs, nregs, cg->trMemory());

   // Then, add all volatile registers to dependencies except for v0 and v1.
   for (int32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastAssignableGPR; i++)
      {
      if ((linkageProp._registerFlags[i] != ARM64_Reserved) && (linkageProp._registerFlags[i] != Preserved))
         {
         auto tmpReg = cg->allocateRegister(TR_GPR);
         TR::addDependency(dependencies, tmpReg, static_cast<TR::RealRegister::RegNum>(i), TR_GPR, cg);
         cg->stopUsingRegister(tmpReg);
         }
      }
   for (int32_t i = TR::RealRegister::v2; i <= TR::RealRegister::LastAssignableFPR; i++)
      {
      if ((linkageProp._registerFlags[i] != ARM64_Reserved) && (linkageProp._registerFlags[i] != Preserved))
         {
         auto tmpReg = cg->allocateRegister(TR_FPR);
         TR::addDependency(dependencies, tmpReg, static_cast<TR::RealRegister::RegNum>(i), TR_FPR, cg);
         cg->stopUsingRegister(tmpReg);
         }
      }

   // Finally add v0 and v1 to dependencies.
   dependencies->addPreCondition(source1Reg, TR::RealRegister::v0);
   dependencies->addPostCondition(trgReg, TR::RealRegister::v0);
   dependencies->addPreCondition(source2Reg, TR::RealRegister::v1);
   auto tmpReg = cg->allocateRegister(TR_FPR);
   dependencies->addPostCondition(tmpReg, TR::RealRegister::v1);
   cg->stopUsingRegister(tmpReg);

   TR::SymbolReference *helperSym = cg->symRefTab()->findOrCreateRuntimeHelper(isSinglePrecision ? TR_ARM64floatRemainder : TR_ARM64doubleRemainder,
                                                                               false, false, false);
   generateImmSymInstruction(cg, TR::InstOpCode::bl, node,
                             (uintptr_t)helperSym->getMethodAddress(),
                             dependencies, helperSym, NULL);
   cg->stopUsingRegister(source1Reg);
   cg->stopUsingRegister(source2Reg);
   cg->decReferenceCount(child1);
   cg->decReferenceCount(child2);
   node->setRegister(trgReg);
   cg->machine()->setLinkRegisterKilled(true);

   return trgReg;
   }
TR::Register *J9::ARM64::TreeEvaluator::fremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return fremHelper(node, cg, true);
   }

TR::Register *J9::ARM64::TreeEvaluator::dremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return fremHelper(node, cg, false);
   }

TR::Register *
J9::ARM64::TreeEvaluator::NULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::evaluateNULLCHKWithPossibleResolve(node, false, cg);
   }

TR::Register *
J9::ARM64::TreeEvaluator::resolveAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::evaluateNULLCHKWithPossibleResolve(node, true, cg);
   }

TR::Register *
J9::ARM64::TreeEvaluator::evaluateNULLCHKWithPossibleResolve(TR::Node *node, bool needsResolve, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::ILOpCode &opCode = firstChild->getOpCode();
   TR::Node *reference = NULL;
   TR::Compilation *comp = cg->comp();

   if (comp->useCompressedPointers() && firstChild->getOpCodeValue() == TR::l2a)
      {
      TR::ILOpCodes loadOp = cg->comp()->il.opCodeForIndirectLoad(TR::Int32);
      TR::ILOpCodes rdbarOp = cg->comp()->il.opCodeForIndirectReadBarrier(TR::Int32);
      TR::Node *n = firstChild;
      while ((n->getOpCodeValue() != loadOp) && (n->getOpCodeValue() != rdbarOp))
         n = n->getFirstChild();
      reference = n->getFirstChild();
      }
   else
      reference = node->getNullCheckReference();

   /* TODO: Resolution */
   /* if(needsResolve) ... */

   // Only explicit test needed for now.
   TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
   TR::Snippet *snippet = new (cg->trHeapMemory()) TR::ARM64HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference(), NULL);
   cg->addSnippet(snippet);
   TR::Register *referenceReg = cg->evaluate(reference);
   TR::Instruction *cbzInstruction = generateCompareBranchInstruction(cg, TR::InstOpCode::cbzx, node, referenceReg, snippetLabel, NULL);
   cbzInstruction->setNeedsGCMap(0xffffffff);
   snippet->gcMap().setGCRegisterMask(0xffffffff);

   if (comp->useCompressedPointers()
         && reference->getOpCodeValue() == TR::l2a)
      {
      TR::Node *n = reference->getFirstChild();
      reference->setIsNonNull(true);
      TR::ILOpCodes loadOp = cg->comp()->il.opCodeForIndirectLoad(TR::Int32);
      TR::ILOpCodes rdbarOp = cg->comp()->il.opCodeForIndirectReadBarrier(TR::Int32);
      while ((n->getOpCodeValue() != loadOp) && (n->getOpCodeValue() != rdbarOp))
         {
         n->setIsNonZero(true);
         n = n->getFirstChild();
         }
      n->setIsNonZero(true);
      }

   reference->setIsNonNull(true);

   /*
   * If the first child is a load with a ref count of 1, just decrement the reference count on the child.
   * If the first child does not have a register, it means it was never evaluated.
   * As a result, the grandchild (the variable reference) needs to be decremented as well.
   *
   * In other cases, evaluate the child node.
   *
   * Under compressedpointers, the first child will have a refCount of at least 2 (the other under an anchor node).
   */
   if (opCode.isLoad() && firstChild->getReferenceCount() == 1
         && !(firstChild->getSymbolReference()->isUnresolved()))
      {
      cg->decReferenceCount(firstChild);
      if (firstChild->getRegister() == NULL)
         {
         cg->decReferenceCount(reference);
         }
      }
   else
      {
      if (comp->useCompressedPointers())
         {
         bool fixRefCount = false;
         if (firstChild->getOpCode().isStoreIndirect()
               && firstChild->getReferenceCount() > 1)
            {
            firstChild->decReferenceCount();
            fixRefCount = true;
            }
         cg->evaluate(firstChild);
         if (fixRefCount)
            firstChild->incReferenceCount();
         }
      else
         cg->evaluate(firstChild);
      cg->decReferenceCount(firstChild);
      }

   return NULL;
   }

TR::Register *J9::ARM64::TreeEvaluator::directCallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::MethodSymbol *callee = symRef->getSymbol()->castToMethodSymbol();
   TR::Linkage      *linkage;
   TR::Register     *returnRegister;

   if (!cg->inlineDirectCall(node, returnRegister))
      {
      if (callee->isJNI() && (node->isPreparedForDirectJNI() || callee->getResolvedMethodSymbol()->canDirectNativeCall()))
         {
         linkage = cg->getLinkage(TR_J9JNILinkage);
         }
      else
         {
         linkage = cg->getLinkage(callee->getLinkageConvention());
         }
      returnRegister = linkage->buildDirectDispatch(node);
      }

   return returnRegister;
   }
