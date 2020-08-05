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

#include <cmath>
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
#include "codegen/Relocation.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "il/AutomaticSymbol.hpp"
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

static TR::Register *
generateSoftwareReadBarrier(TR::Node *node, TR::CodeGenerator *cg, bool isArdbari)
   {
#ifndef OMR_GC_CONCURRENT_SCAVENGER
   TR_ASSERT_FATAL(false, "Concurrent Scavenger not supported.");
#else
   TR::Compilation *comp = cg->comp();
   TR::MemoryReference *tempMR = NULL;

   TR::Register *tempReg;
   TR::Register *locationReg = cg->allocateRegister();
   TR::Register *evacuateReg = cg->allocateRegister();
   TR::Register *x0Reg = cg->allocateRegister();
   TR::Register *vmThreadReg = cg->getMethodMetaDataRegister();

   if (!node->getSymbolReference()->getSymbol()->isInternalPointer())
      {
      if (node->getSymbolReference()->getSymbol()->isNotCollected())
         tempReg = cg->allocateRegister();
      else
         tempReg = cg->allocateCollectedReferenceRegister();
      }
   else
      {
      tempReg = cg->allocateRegister();
      tempReg->setPinningArrayPointer(node->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
      tempReg->setContainsInternalPointer();
      }

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
   startLabel->setStartInternalControlFlow();
   endLabel->setEndInternalControlFlow();

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg->trMemory());
   deps->addPostCondition(tempReg, TR::RealRegister::NoReg);
   deps->addPostCondition(locationReg, TR::RealRegister::x1); // TR_softwareReadBarrier helper needs this in x1.
   deps->addPostCondition(evacuateReg, TR::RealRegister::NoReg);
   deps->addPostCondition(x0Reg, TR::RealRegister::x0);

   node->setRegister(tempReg);

   tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, cg);
   if (tempMR->getUnresolvedSnippet() != NULL)
      {
      generateTrg1MemInstruction(cg, TR::InstOpCode::addx, node, locationReg, tempMR);
      }
   else
      {
      if (tempMR->useIndexedForm())
         generateTrg1Src2Instruction(cg, TR::InstOpCode::addx, node, locationReg, tempMR->getBaseRegister(), tempMR->getIndexRegister());
      else
         generateTrg1MemInstruction(cg, TR::InstOpCode::addimmx, node, locationReg, tempMR);
      }

   TR::InstOpCode::Mnemonic loadOp = isArdbari ? TR::InstOpCode::ldrimmx : TR::InstOpCode::ldrimmw;

   generateTrg1MemInstruction(cg, loadOp, node, tempReg, new (cg->trHeapMemory()) TR::MemoryReference(locationReg, 0, cg));

   if (isArdbari && node->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef())
      TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, tempReg);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, startLabel);

   generateTrg1MemInstruction(cg, loadOp, node, evacuateReg,
         new (cg->trHeapMemory()) TR::MemoryReference(vmThreadReg, comp->fej9()->thisThreadGetEvacuateBaseAddressOffset(), cg));
   generateCompareInstruction(cg, node, tempReg, evacuateReg, isArdbari); // 64-bit compare in ardbari
   generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, endLabel, TR::CC_LT);

   generateTrg1MemInstruction(cg, loadOp, node, evacuateReg,
         new (cg->trHeapMemory()) TR::MemoryReference(vmThreadReg, comp->fej9()->thisThreadGetEvacuateTopAddressOffset(), cg));
   generateCompareInstruction(cg, node, tempReg, evacuateReg, isArdbari); // 64-bit compare in ardbari
   generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, endLabel, TR::CC_GT);

   // TR_softwareReadBarrier helper expects the vmThread in x0.
   generateMovInstruction(cg, node, x0Reg, vmThreadReg);

   TR::SymbolReference *helperSym = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_softwareReadBarrier, false, false, false);
   generateImmSymInstruction(cg, TR::InstOpCode::bl, node, (uintptr_t)helperSym->getMethodAddress(), deps, helperSym, NULL);

   generateTrg1MemInstruction(cg, loadOp, node, tempReg, new (cg->trHeapMemory()) TR::MemoryReference(locationReg, 0, cg));

   if (isArdbari && node->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef())
      TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, tempReg);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, endLabel, deps);

   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && comp->target().isSMP());
   if (needSync)
      {
      generateSynchronizationInstruction(cg, TR::InstOpCode::dsb, node, 0xF); // dsb SY
      }

   tempMR->decNodeReferenceCounts(cg);

   cg->stopUsingRegister(evacuateReg);
   cg->stopUsingRegister(locationReg);
   cg->stopUsingRegister(x0Reg);

   cg->machine()->setLinkRegisterKilled(true);

   return tempReg;
#endif // OMR_GC_CONCURRENT_SCAVENGER
   }

TR::Register *
J9::ARM64::TreeEvaluator::irdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Node *sideEffectNode = node->getFirstChild();
   TR::Register * sideEffectRegister = cg->evaluate(sideEffectNode);
   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
      }
   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::iloadEvaluator(node, cg);
   }

TR::Register *
J9::ARM64::TreeEvaluator::irdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Node *sideEffectNode = node->getFirstChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);
   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
      }

   // Note: For indirect rdbar nodes, the first child (sideEffectNode) is also used by the
   // load evaluator. The load evaluator will also evaluate+decrement it. In order to avoid double
   // decrementing the node we skip doing it here and let the load evaluator do it.
   if (TR::Compiler->om.readBarrierType() != gc_modron_readbar_none &&
       cg->comp()->useCompressedPointers() &&
       (node->getOpCode().hasSymbolReference() &&
        node->getSymbolReference()->getSymbol()->getDataType() == TR::Address))
      {
      return generateSoftwareReadBarrier(node, cg, false);
      }
   else
      return TR::TreeEvaluator::iloadEvaluator(node, cg);
   }

TR::Register *
J9::ARM64::TreeEvaluator::ardbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Node *sideEffectNode = node->getFirstChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);
   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR_ASSERT_FATAL(false, "Field watch not supported");
      // TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
      }
   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::aloadEvaluator(node, cg);
   }

TR::Register *
J9::ARM64::TreeEvaluator::ardbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *sideEffectRegister = cg->evaluate(node->getFirstChild());
   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR_ASSERT_FATAL(false, "Field watch not supported");
      // TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
      }
   // Note: For indirect rdbar nodes, the first child (sideEffectNode) is also used by the
   // load evaluator. The load evaluator will also evaluate+decrement it. In order to avoid double
   // decrementing the node we skip doing it here and let the load evaluator do it.
   if (TR::Compiler->om.readBarrierType() == gc_modron_readbar_none)
      return TR::TreeEvaluator::aloadEvaluator(node, cg);
   else
      return generateSoftwareReadBarrier(node, cg, true);
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
      generateCompareBranchInstruction(cg, TR::InstOpCode::cbzx, node, srcReg, doneLabel);
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
   // ARM64HelperCallSnippet generates "bl" instruction
   cg->machine()->setLinkRegisterKilled(true);

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

   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, cg);

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

   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, cg);

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
         gcPoint = generateCompareBranchInstruction(cg, compareOp, node, divisorReg, snippetLabel);
         }
      gcPoint->ARM64NeedsGCMap(cg, 0xffffffff);
      snippet->gcMap().setGCRegisterMask(0xffffffff);
      }

   cg->evaluate(node->getFirstChild());
   cg->decReferenceCount(node->getFirstChild());
   // ARM64HelperCallSnippet generates "bl" instruction
   cg->machine()->setLinkRegisterKilled(true);
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
       comp->getOption(TR_DisableInlineMonExit) ||
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

   cg->stopUsingRegister(dataReg);
   cg->stopUsingRegister(addrReg);
   cg->stopUsingRegister(tempReg);

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

   TR_ASSERT(testNode->getOpCodeValue() == TR::lcmpeq && secondChild->getLongInt() == -1L, "asynccheck bad format");

   TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   TR::SymbolReference *asynccheckHelper = node->getSymbolReference();
   TR::Snippet *snippet = new (cg->trHeapMemory()) TR::ARM64HelperCallSnippet(cg, node, snippetLabel, asynccheckHelper, doneLabel);
   cg->addSnippet(snippet);

   generateCompareImmInstruction(cg, node, src1Reg, secondChild->getLongInt(), true); // 64-bit compare

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

/**
 * @brief Generates instructions for allocating heap for new/newarray/anewarray
 *        The limitation of the current implementation:
 *          - supports `new` only
 *          - does not support dual TLH
 *          - does not support realtimeGC
 *
 * @param[in] node:          node
 * @param[in] cg:            code generator
 * @param[in] isVariableLen: true if allocating variable length array
 * @param[in] allocSize:     size to allocate on heap if isVariableLen is false. offset to data start if isVariableLen is true.
 * @param[in] elementSize:   size of array elements. Used if isVariableLen is true.
 * @param[in] resultReg:     the register that contains allocated heap address
 * @param[in] lengthReg:     the register that contains array length (number of elements). Used if isVariableLen is true.
 * @param[in] heapTopReg:    temporary register 1
 * @param[in] tempReg:       temporary register 2
 * @param[in] dataSizeReg:   temporary register 3, this register contains the number of allocated bytes if isVariableLen is true.
 * @param[in] conditions:    dependency conditions
 * @param[in] callLabel:     label to call when allocation fails
 */
static void
genHeapAlloc(TR::Node *node, TR::CodeGenerator *cg, bool isVariableLen, uint32_t allocSize, int32_t elementSize, TR::Register *resultReg,
   TR::Register *lengthReg, TR::Register *heapTopReg, TR::Register *tempReg, TR::Register *dataSizeReg, TR::RegisterDependencyConditions *conditions,
   TR::LabelSymbol *callLabel)
   {
   TR::Compilation *comp = cg->comp();
   TR::Register *metaReg = cg->getMethodMetaDataRegister();

   uint32_t maxSafeSize = cg->getMaxObjectSizeGuaranteedNotToOverflow();

   static_assert(offsetof(J9VMThread, heapAlloc) < 32760, "Expecting offset to heapAlloc fits in imm12");
   static_assert(offsetof(J9VMThread, heapTop) < 32760, "Expecting offset to heapTop fits in imm12");

   if (isVariableLen)
      {
      /*
       * Instructions for allocating heap for variable length `newarray/anewarray`.
       *
       * cmp      lengthReg, #maxObjectSizeInElements
       * b.hi     callLabel
       * 
       * uxtw     tempReg, lengthReg
       * ldrimmx  resultReg, [metaReg, offsetToHeapAlloc]
       * lsl      tempReg, lengthReg, #shiftValue
       * addimmx  tempReg, tempReg, #offset+round-1
       * cmpimmw  lengthReg, 0; # of array elements
       * andimmx  tempReg, tempReg, #-round
       * movzx    tempReg2, #sizeOfDiscontiguousArrayHeader
       * cselx    dataSizeReg, tempReg, tempReg2, ne
       * ldrimmx  heapTopReg, [metaReg, offsetToHeapTop]
       * addimmx  tempReg, resultReg, dataSizeReg
       * 
       * # check for overflow
       * cmp      tempReg, heapTopReg
       * b.gt     callLabel
       * # write back heapAlloc
       * strimmx  tempReg, [metaReg, offsetToHeapAlloc]
       *
       */
      // Detect large or negative number of elements in case addr wrap-around
      // 
      // The GC will guarantee that at least 'maxObjectSizeGuaranteedNotToOverflow' bytes
      // of slush will exist between the top of the heap and the end of the address space.
      //
      uint32_t maxObjectSizeInElements = maxSafeSize / elementSize;
      if (constantIsUnsignedImm12(maxObjectSizeInElements))
         {
         generateCompareImmInstruction(cg, node, lengthReg, maxObjectSizeInElements, false);
         }
      else
         {
         loadConstant32(cg, node, maxObjectSizeInElements, tempReg);
         generateCompareInstruction(cg, node, lengthReg, tempReg, false);
         }
      // Must be an unsigned comparison on sizes.
      //
      generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, callLabel, TR::CC_HI, conditions);

      // At this point, lengthReg must contain non-negative value.
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ubfmx, node, tempReg, lengthReg, 31); // uxtw

      // Load the base of the next available heap storage.
      generateTrg1MemInstruction(cg,TR::InstOpCode::ldrimmx, node, resultReg,
            new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapAlloc), cg));

      // calculate variable size, rounding up if necessary to a intptr_t multiple boundary
      //
      // zero indicates no rounding is necessary
      const int32_t round = (elementSize >= TR::Compiler->om.objectAlignmentInBytes()) ? 0 : TR::Compiler->om.objectAlignmentInBytes();

      // If the array is zero length, the array is a discontiguous.
      // Large heap builds do not need to care about this because the
      // contiguous and discontiguous array headers are the same size.
      //
      auto shiftAmount = trailingZeroes(elementSize);
      auto displacement = (round > 0) ? round - 1 : 0;
      uint32_t alignmentMaskEncoding;
      bool maskN;

      if (round != 0)
         {
         if (round == 8)
            {
            maskN = true;
            alignmentMaskEncoding = 0xf7c;
            }
         else
            {
            bool canBeEncoded = logicImmediateHelper(-round, true, maskN, alignmentMaskEncoding);
            TR_ASSERT_FATAL(canBeEncoded, "mask for andimmx (%d) cannnot be encoded", (-round));
            }
         }
      if (comp->useCompressedPointers())
         {
         if (shiftAmount > 0)
            {
            generateLogicalShiftLeftImmInstruction(cg, node, tempReg, tempReg, shiftAmount, true);
            }
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addimmx, node, tempReg, tempReg, (allocSize + displacement));
         generateCompareImmInstruction(cg, node, lengthReg, 0, false); // lengthReg is 32bit
         if (round != 0)
            {
            generateLogicalImmInstruction(cg, TR::InstOpCode::andimmx, node, tempReg, tempReg, maskN, alignmentMaskEncoding);
            }
         loadConstant64(cg, node, TR::Compiler->om.discontiguousArrayHeaderSizeInBytes(), heapTopReg);

         generateCondTrg1Src2Instruction(cg, TR::InstOpCode::cselx, node, dataSizeReg, tempReg, heapTopReg, TR::CC_NE); 
         }
      else
         {
         if (shiftAmount > 0)
            {
            generateLogicalShiftLeftImmInstruction(cg, node, tempReg, tempReg, shiftAmount, false);
            }
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addimmx, node, dataSizeReg, tempReg, (allocSize + displacement));
         if (round != 0)
            {
            generateLogicalImmInstruction(cg, TR::InstOpCode::andimmx, node, dataSizeReg, dataSizeReg, maskN, alignmentMaskEncoding);
            }
         }

      // Load the heap top
      generateTrg1MemInstruction(cg,TR::InstOpCode::ldrimmx, node, heapTopReg,
               new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapTop), cg));
      generateTrg1Src2Instruction(cg, TR::InstOpCode::addx, node, tempReg, resultReg, dataSizeReg);

      }
   else
      {
      /*
       * Instructions for allocating heap for fixed length `new/newarray/anewarray`.
       *
       * ldrimmx  resultReg, [metaReg, offsetToHeapAlloc]
       * ldrimmx  heapTopReg, [metaReg, offsetToHeapTop]
       * addsimmx tempReg, resultReg, #allocSize
       * # check for address wrap-around if necessary
       * b.cc     callLabel
       * # check for overflow
       * cmp      tempReg, heapTopReg
       * b.gt     callLabel
       * # write back heapAlloc
       * strimmx  tempReg, [metaReg, offsetToHeapAlloc]
       *
       */

      // Load the base of the next available heap storage.
      generateTrg1MemInstruction(cg,TR::InstOpCode::ldrimmx, node, resultReg,
            new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapAlloc), cg));
      // Load the heap top
      generateTrg1MemInstruction(cg,TR::InstOpCode::ldrimmx, node, heapTopReg,
               new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapTop), cg));

      // Calculate the after-allocation heapAlloc: if the size is huge,
      // we need to check address wrap-around also. This is unsigned
      // integer arithmetic, checking carry bit is enough to detect it.
      const bool isAllocSizeInReg = !constantIsUnsignedImm12(allocSize);
      const bool isWithinMaxSafeSize = allocSize <= maxSafeSize;
      if (isAllocSizeInReg)
         {
         loadConstant64(cg, node, allocSize, tempReg);
         generateTrg1Src2Instruction(cg, isWithinMaxSafeSize ? TR::InstOpCode::addx : TR::InstOpCode::addsx,
                     node, tempReg, resultReg, tempReg);
         }
      else
         {
         generateTrg1Src1ImmInstruction(cg, isWithinMaxSafeSize ? TR::InstOpCode::addimmx : TR::InstOpCode::addsimmx,
                     node, tempReg, resultReg, allocSize);
         }
      if (!isWithinMaxSafeSize)
         {
         generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, callLabel, TR::CC_CC, conditions);
         }

      }

   // Ok, tempReg now points to where the object will end on the TLH.
   // resultReg will contain the start of the object where we'll write out our
   // J9Class*. Should look like this in memory:
   // [heapAlloc == resultReg] ... tempReg ...//... heapTopReg.

   //Here we check if we overflow the TLH Heap Top
   //branch to heapAlloc Snippet if we overflow (ie callLabel).
   generateCompareInstruction(cg, node, tempReg, heapTopReg, true);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, callLabel, TR::CC_GT, conditions);

   //Done, write back to heapAlloc here.
   generateMemSrc1Instruction(cg, TR::InstOpCode::strimmx, node,
         new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapAlloc), cg), tempReg);

   }

/**
 * @brief Generates instructions for initializing allocated memory for new/newarray/anewarray
 *
 * @param[in] node:          node
 * @param[in] cg:            code generator
 * @param[in] isVariableLen: true if allocating variable length array
 * @param[in] objectSize:    size of the object
 * @param[in] headerSize:    header size of the object
 * @param[in] objectReg:     the register that holds object address
 * @param[in] dataSizeReg:   the register that holds the number of allocated bytes if isVariableLength is true
 * @param[in] zeroReg:       the register whose value is zero
 * @param[in] tempReg1:      temporary register 1
 * @param[in] tempReg2:      temporary register 2
 */
static void
genZeroInitObject(TR::Node *node, TR::CodeGenerator *cg, bool isVariableLen, uint32_t objectSize, uint32_t headerSize, TR::Register *objectReg,
   TR::Register *dataSizeReg, TR::Register *zeroReg, TR::Register *tempReg1, TR::Register *tempReg2)
   {

   if (isVariableLen)
      {
      /*
       * Instructions for clearing allocated memory for variable length
       * We assume that the objectSize is multiple of 8.
       * Because the size of the header of contiguous arrays are multiple of 8,
       * the data size to clear is also multiple of 8.
       *
       * subimmx dataSizeReg, dataSizeReg, #headerSize
       * cbz     dataSizeReg, zeroinitdone
       * // Adjust tempReg1 so that (tempReg1 + 16) points to
       * // the memory area beyond the object header
       * subimmx tempReg1, objectReg, (16 - #headerSize)
       * cmp     dataSizeReg, #64
       * b.lt    medium
       * large:  // dataSizeReg >= 64
       * lsr     tempReg2, dataSizeReg, #6 // loopCount = dataSize / 64
       * and     dataSizeReg, dataSizeReg, #63
       * loopStart:
       * stpimmx xzr, xzr, [tempReg1, #16]
       * stpimmx xzr, xzr, [tempReg1, #32]
       * stpimmx xzr, xzr, [tempReg1, #48]
       * stpimmx xzr, xzr, [tempReg1, #64]! // pre index
       * subsimmx tempReg2, tempReg2, #1
       * b.ne    loopStart
       * cbz     dataSizeReg, zeroinitdone
       * medium:
       * addx    tempReg2, tempReg1, dataSizeReg // tempReg2 points to 16bytes before the end of the buffer
       * // write residues. We have at least 8bytes before (tempReg1 + 16)
       * cmpimmx dataSizeReg, #16
       * b.le    write16
       * cmpimmx dataSizeReg, #32
       * b.le    write32
       * cmpimmx dataSizeReg, #48
       * b.le    write48
       * write64: // 56 bytes
       * stpimmx xzr, xzr, [tempReg2, #-48]
       * write48: // 40, 48 bytes
       * stpimmx xzr, xzr, [tempReg2, #-32]
       * write32: // 24, 32 bytes
       * stpimmx xzr, xzr, [tempReg2, #-16]
       * write16: // 8, 16 bytes
       * stpimmx xzr, xzr, [tempReg2]
       * zeroinitdone:
       */
      TR::LabelSymbol *zeroInitDoneLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *mediumLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *loopStartLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *write16Label = generateLabelSymbol(cg);
      TR::LabelSymbol *write32Label = generateLabelSymbol(cg);
      TR::LabelSymbol *write48Label = generateLabelSymbol(cg);

      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::subimmx, node, dataSizeReg, dataSizeReg, headerSize);
      if (!TR::Compiler->om.generateCompressedObjectHeaders())
         {
         // Array Header is smaller than the minimum data size in compressedrefs build, so this check is not necessary.
         // This check is necessary in large heap build.
         generateCompareBranchInstruction(cg, TR::InstOpCode::cbzx, node, dataSizeReg, zeroInitDoneLabel);
         }
      generateTrg1Src1ImmInstruction(cg, (headerSize > 16) ? TR::InstOpCode::addimmx : TR::InstOpCode::subimmx,
         node, tempReg1, objectReg, std::abs(static_cast<int>(headerSize - 16)));

      generateCompareImmInstruction(cg, node, dataSizeReg, 64, true);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, mediumLabel, TR::CC_LT);
      generateLogicalShiftRightImmInstruction(cg, node, tempReg2, dataSizeReg, 6, true);
      generateLogicalImmInstruction(cg, TR::InstOpCode::andimmx, node, dataSizeReg, dataSizeReg, true, 5); // N = true, immr:imms = 5

      generateLabelInstruction(cg, TR::InstOpCode::label, node, loopStartLabel);
      generateMemSrc2Instruction(cg, TR::InstOpCode::stpoffx, node, new (cg->trHeapMemory()) TR::MemoryReference(tempReg1, 16, cg), zeroReg, zeroReg);
      generateMemSrc2Instruction(cg, TR::InstOpCode::stpoffx, node, new (cg->trHeapMemory()) TR::MemoryReference(tempReg1, 32, cg), zeroReg, zeroReg);
      generateMemSrc2Instruction(cg, TR::InstOpCode::stpoffx, node, new (cg->trHeapMemory()) TR::MemoryReference(tempReg1, 48, cg), zeroReg, zeroReg);
      generateMemSrc2Instruction(cg, TR::InstOpCode::stpprex, node, new (cg->trHeapMemory()) TR::MemoryReference(tempReg1, 64, cg), zeroReg, zeroReg);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::subsimmx, node, tempReg2, tempReg2, 1);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, loopStartLabel, TR::CC_NE);

      generateLabelInstruction(cg, TR::InstOpCode::label, node, mediumLabel);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::addx, node, tempReg2, tempReg1, dataSizeReg);
      generateCompareImmInstruction(cg, node, dataSizeReg, 16, true);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, write16Label, TR::CC_LE);
      generateCompareImmInstruction(cg, node, dataSizeReg, 32, true);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, write32Label, TR::CC_LE);
      generateCompareImmInstruction(cg, node, dataSizeReg, 48, true);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, write48Label, TR::CC_LE);

      generateMemSrc2Instruction(cg, TR::InstOpCode::stpoffx, node, new (cg->trHeapMemory()) TR::MemoryReference(tempReg2, -48, cg), zeroReg, zeroReg);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, write48Label);
      generateMemSrc2Instruction(cg, TR::InstOpCode::stpoffx, node, new (cg->trHeapMemory()) TR::MemoryReference(tempReg2, -32, cg), zeroReg, zeroReg);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, write32Label);
      generateMemSrc2Instruction(cg, TR::InstOpCode::stpoffx, node, new (cg->trHeapMemory()) TR::MemoryReference(tempReg2, -16, cg), zeroReg, zeroReg);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, write16Label);
      generateMemSrc2Instruction(cg, TR::InstOpCode::stpoffx, node, new (cg->trHeapMemory()) TR::MemoryReference(tempReg2, 0, cg), zeroReg, zeroReg);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, zeroInitDoneLabel);
      }
   else
      {
      /*
       * Instructions for clearing allocated memory for fixed length
       * We assume that the objectSize is multiple of 4.
       *
       * // Adjust tempReg1 so that (tempReg1 + 16) points to
       * // the memory area beyond the object header
       * subimmx tempReg1, objectReg, (16 - #headerSize)
       * movzx   tempReg2, loopCount
       * loop:
       * stpimmx xzr, xzr, [tempReg1, #16]
       * stpimmx xzr, xzr, [tempReg1, #32]
       * stpimmx xzr, xzr, [tempReg1, #48]
       * stpimmx xzr, xzr, [tempReg1, #64]! // pre index
       * subsimmx tempReg2, tempReg2, #1
       * b.ne    loop
       * // write residues
       * stpimmx xzr, xzr [tempReg1, #16]
       * stpimmx xzr, xzr [tempReg1, #32]
       * stpimmx xzr, xzr [tempReg1, #48]
       * strimmx xzr, [tempReg1, #64]
       * strimmw xzr, [tempReg1, #72]
       *
       */
      // TODO align tempReg1 to 16-byte boundary if objectSize is large
      // TODO use vector register
      // TODO use dc zva
      const int32_t unrollFactor = 4;
      const int32_t width = 16; // use stp to clear 16 bytes
      const int32_t loopCount = (objectSize - headerSize) / (unrollFactor * width);
      const int32_t res1 = (objectSize - headerSize) % (unrollFactor * width);
      const int32_t residueCount = res1 / width;
      const int32_t res2 = res1 % width;
      TR::LabelSymbol *loopStart = generateLabelSymbol(cg);

      generateTrg1Src1ImmInstruction(cg, (headerSize > 16) ? TR::InstOpCode::addimmx : TR::InstOpCode::subimmx,
            node, tempReg1, objectReg, std::abs(static_cast<int>(headerSize - 16)));

      if (loopCount > 0)
         {
         if (loopCount > 1)
            {
            loadConstant64(cg, node, loopCount, tempReg2);
            generateLabelInstruction(cg, TR::InstOpCode::label, node, loopStart);
            }
         for (int i = 1; i < unrollFactor; i++)
            {
            generateMemSrc2Instruction(cg, TR::InstOpCode::stpoffx, node, new (cg->trHeapMemory()) TR::MemoryReference(tempReg1, i * width, cg), zeroReg, zeroReg);
            }
         generateMemSrc2Instruction(cg, TR::InstOpCode::stpprex, node, new (cg->trHeapMemory()) TR::MemoryReference(tempReg1, unrollFactor * width, cg), zeroReg, zeroReg);
         if (loopCount > 1)
            {
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::subsimmx, node, tempReg2, tempReg2, 1);
            generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, loopStart, TR::CC_NE);
            }
         }
      for (int i = 0; i < residueCount; i++)
         {
         generateMemSrc2Instruction(cg, TR::InstOpCode::stpoffx, node, new (cg->trHeapMemory()) TR::MemoryReference(tempReg1, (i + 1) * width, cg), zeroReg, zeroReg);
         }
      int offset = (residueCount + 1) * width;
      if (res2 >= 8)
         {
         generateMemSrc1Instruction(cg, TR::InstOpCode::strimmx, node, new (cg->trHeapMemory()) TR::MemoryReference(tempReg1, offset, cg), zeroReg);
         offset += 8;
         }
      if ((res2 & 4) > 0)
         {
         generateMemSrc1Instruction(cg, TR::InstOpCode::strimmw, node, new (cg->trHeapMemory()) TR::MemoryReference(tempReg1, offset, cg), zeroReg);
         }

      }
   }

/**
 * @brief Generates instructions for initializing Object header for new/newarray/anewarray
 *
 * @param[in] node:       node
 * @param[in] cg:         code generator
 * @param[in] clazz:      class pointer to store in the object header
 * @param[in] objectReg:  the register that holds object address
 * @param[in] classReg:   the register that holds class
 * @param[in] tempReg1:   temporary register 1
 */
static void
genInitObjectHeader(TR::Node *node, TR::CodeGenerator *cg, TR_OpaqueClassBlock *clazz, TR::Register *objectReg, TR::Register *classReg, TR::Register *tempReg1)
   {
   TR_ASSERT(clazz, "Cannot have a null OpaqueClassBlock\n");
   TR_J9VM *fej9 = reinterpret_cast<TR_J9VM *>(cg->fe());
   TR::Compilation *comp = cg->comp();
   TR::Register * clzReg = classReg;
   TR::Register *metaReg = cg->getMethodMetaDataRegister();

   // For newarray/anewarray, classReg holds the class pointer of array elements
   // Prepare valid class pointer for arrays
   if (node->getOpCodeValue() != TR::New)
      {
      if (cg->needClassAndMethodPointerRelocations())
         {
         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            loadAddressConstantInSnippet(cg, node, reinterpret_cast<intptr_t>(clazz), tempReg1, TR_ClassPointer);
            }
         else
            {
            if (node->getOpCodeValue() == TR::newarray)
               {
               generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, tempReg1,
                  new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, javaVM), cg));
               generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, tempReg1,
                  new (cg->trHeapMemory()) TR::MemoryReference(tempReg1,
                     fej9->getPrimitiveArrayOffsetInJavaVM(node->getSecondChild()->getInt()), cg));
               }
            else
               {
               generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, tempReg1,
                  new (cg->trHeapMemory()) TR::MemoryReference(classReg, offsetof(J9Class, arrayClass), cg));
               }
            }
         }
      else
         {
         loadAddressConstant(cg, node, reinterpret_cast<intptr_t>(clazz), tempReg1);
         }
      clzReg = tempReg1;
      }

   // Store the class
   generateMemSrc1Instruction(cg, TR::Compiler->om.generateCompressedObjectHeaders() ? TR::InstOpCode::strimmw : TR::InstOpCode::strimmx,
      node, new (cg->trHeapMemory()) TR::MemoryReference(objectReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg), clzReg);
   }

/**
 * @brief Generates instructions for initializing array header for newarray/anewarray
 *
 * @param[in] node:       node
 * @param[in] cg:         code generator
 * @param[in] clazz:      class pointer to store in the object header
 * @param[in] objectReg:  the register that holds object address
 * @param[in] classReg:   the register that holds class
 * @param[in] sizeReg:    the register that holds array length.
 * @param[in] zeroReg:    the register whose value is zero
 * @param[in] tempReg1:   temporary register 1
 */
static void
genInitArrayHeader(TR::Node *node, TR::CodeGenerator *cg, TR_OpaqueClassBlock *clazz, TR::Register *objectReg, TR::Register *classReg, TR::Register *sizeReg, TR::Register *zeroReg, TR::Register *tempReg1)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());

   genInitObjectHeader(node, cg, clazz, objectReg, classReg, tempReg1);
   if (node->getFirstChild()->getOpCode().isLoadConst() && (node->getFirstChild()->getInt() == 0))
      {
      // constant zero length array
      // Zero length arrays are discontiguous (i.e. they also need the discontiguous length field to be 0) because
      // they are indistinguishable from non-zero length discontiguous arrays
      if (TR::Compiler->om.generateCompressedObjectHeaders())
         {
         // `mustBeZero` and `size` field of J9IndexableObjectDiscontiguousCompressed must be cleared.
         // We cannot use `strimmx` in this case because offset would be 4 bytes, which cannot be encoded as imm12 of `strimmx`.
         generateMemSrc1Instruction(cg, TR::InstOpCode::strimmw, node,
                                    new (cg->trHeapMemory()) TR::MemoryReference(objectReg, fej9->getOffsetOfDiscontiguousArraySizeField() - 4, cg),
                                    zeroReg);
         generateMemSrc1Instruction(cg, TR::InstOpCode::strimmw, node,
                                    new (cg->trHeapMemory()) TR::MemoryReference(objectReg, fej9->getOffsetOfDiscontiguousArraySizeField(), cg),
                                    zeroReg);
         }
      else
         {
         // `strimmx` can be used as offset is 8 bytes.
         generateMemSrc1Instruction(cg, TR::InstOpCode::strimmx, node,
                                    new (cg->trHeapMemory()) TR::MemoryReference(objectReg, fej9->getOffsetOfDiscontiguousArraySizeField() - 4, cg),
                                    zeroReg);
         }
      }
   else
      {
      // Store the array size
      // In the compressedrefs build, size field of discontigous array header is cleared by instructions generated by genZeroInit().
      // In the large heap build, we must clear size and mustBeZero field here
      if (TR::Compiler->om.generateCompressedObjectHeaders())
         {
         generateMemSrc1Instruction(cg, TR::InstOpCode::strimmw, node,
                                    new (cg->trHeapMemory()) TR::MemoryReference(objectReg, fej9->getOffsetOfContiguousArraySizeField(), cg),
                                    sizeReg);
         }
      else
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ubfmx, node, tempReg1, sizeReg, 31); // uxtw
         generateMemSrc1Instruction(cg, TR::InstOpCode::strimmx, node,
                                    new (cg->trHeapMemory()) TR::MemoryReference(objectReg, fej9->getOffsetOfContiguousArraySizeField(), cg),
                                    tempReg1);
         }
      }
   }

/**
 * @brief Generates instructions for inlining new/newarray/anewarray
 *        The limitation of the current implementation:
 *          - supports `new` only
 *          - does not support dual TLH
 *          - does not support realtimeGC
 *
 * @param node: node
 * @param cg: code generator
 *
 * @return register containing allocated object, NULL if inlining is not possible
 */
TR::Register *
J9::ARM64::TreeEvaluator::VMnewEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());

   if (comp->suppressAllocationInlining())
      return NULL;

   if (comp->getOption(TR_DisableTarokInlineArrayletAllocation) && (node->getOpCodeValue() == TR::anewarray || node->getOpCodeValue() == TR::newarray))
      return NULL;

   // Currently, we do not support realtime GC.
   if (comp->getOptions()->realTimeGC())
      return NULL;

   TR_OpaqueClassBlock *clazz      = NULL;

   // --------------------------------------------------------------------------------
   //
   // Find the class info and allocation size depending on the node type.
   //
   // Returns:
   //    size of object    includes the size of the array header
   //    -1                cannot allocate inline
   //    0                 variable sized allocation
   //
   // --------------------------------------------------------------------------------

   int32_t objectSize = comp->canAllocateInline(node, clazz);
   if (objectSize < 0)
      return NULL;
   const bool isVariableLength = (objectSize == 0);

   static long count = 0;
   if (!performTransformation(comp, "O^O <%3d> Inlining Allocation of %s [0x%p].\n", count++, node->getOpCode().getName(), node))
      return NULL;


   // 1. Evaluate children
   int32_t headerSize;
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = NULL;
   int32_t elementSize = 0;
   bool isArrayNew = false;
   TR::Register *classReg = NULL;
   TR::Register *lengthReg = NULL;
   TR::ILOpCodes opCode = node->getOpCodeValue();
   if (opCode == TR::New)
      {
      // classReg is passed to the VM helper on the slow path and subsequently clobbered; copy it for later nodes if necessary
      classReg = cg->gprClobberEvaluate(firstChild);
      headerSize = TR::Compiler->om.objectHeaderSizeInBytes();
      lengthReg = cg->allocateRegister();
      }
   else
      {
      elementSize = TR::Compiler->om.getSizeOfArrayElement(node);
 
      // If the array cannot be allocated as a contiguous array, then comp->canAllocateInline should have returned -1.
      // The only exception is when the array length is 0.
      headerSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
      isArrayNew = true;

      lengthReg = cg->evaluate(firstChild);
      secondChild = node->getSecondChild();
      // classReg is passed to the VM helper on the slow path and subsequently clobbered; copy it for later nodes if necessary
      classReg = cg->gprClobberEvaluate(secondChild);
      }

   TR::Instruction *firstInstructionAfterClassAndLengthRegsAreReady = cg->getAppendInstruction();
   // 2. Calculate allocation size
   int32_t allocateSize = isVariableLength ? headerSize : (objectSize + TR::Compiler->om.objectAlignmentInBytes() - 1) & (-TR::Compiler->om.objectAlignmentInBytes());

   // 3. Allocate registers
   TR::Register *resultReg = cg->allocateRegister();
   TR::Register *tempReg1 = cg->allocateRegister();
   TR::Register *tempReg2 = cg->allocateRegister();
   TR::Register *tempReg3 = isVariableLength ? cg->allocateRegister() : NULL;
   TR::Register *zeroReg = cg->allocateRegister();
   TR::LabelSymbol *callLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *callReturnLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);

   // 4. Setup register dependencies
   const int numReg = isVariableLength ? 7 : 6;
   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(numReg, numReg, cg->trMemory());
   TR::addDependency(conditions, classReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, resultReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, lengthReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, zeroReg, TR::RealRegister::xzr, TR_GPR, cg);
   TR::addDependency(conditions, tempReg1, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, tempReg2, TR::RealRegister::NoReg, TR_GPR, cg);
   if (isVariableLength)
      {
      TR::addDependency(conditions, tempReg3, TR::RealRegister::NoReg, TR_GPR, cg);
      }

   // 5. Allocate object/array on heap
   genHeapAlloc(node, cg, isVariableLength, allocateSize, elementSize, resultReg, lengthReg, tempReg1, tempReg2, tempReg3, conditions, callLabel);

   // 6. Setup OOL Section for slowpath
   TR::Register *objReg = cg->allocateCollectedReferenceRegister();
   TR_ARM64OutOfLineCodeSection *outlinedHelperCall = new (cg->trHeapMemory()) TR_ARM64OutOfLineCodeSection(node, TR::acall, objReg, callLabel, callReturnLabel, cg);
   cg->getARM64OutOfLineCodeSectionList().push_front(outlinedHelperCall);

   // 7. Initialize the allocated memory area with zero
   // TODO selectively initialize necessary slots
   genZeroInitObject(node, cg, isVariableLength, objectSize, headerSize, resultReg, tempReg3, zeroReg, tempReg1, tempReg2);

   // 8. Initialize Object Header
   if (isArrayNew)
      {
      genInitArrayHeader(node, cg, clazz, resultReg, classReg, lengthReg, zeroReg, tempReg1);
      }
   else
      {
      genInitObjectHeader(node, cg, clazz, resultReg, classReg, tempReg1);
      }

   // 9. Setup AOT relocation
   if (cg->comp()->compileRelocatableCode() && (opCode == TR::New || opCode == TR::anewarray))
      {
      TR::Instruction *firstInstruction = firstInstructionAfterClassAndLengthRegsAreReady->getNext();
      TR_OpaqueClassBlock *classToValidate = clazz;

      TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation *) comp->trMemory()->allocateMemory(sizeof(TR_RelocationRecordInformation), heapAlloc);
      recordInfo->data1 = allocateSize;
      recordInfo->data2 = node->getInlinedSiteIndex();
      recordInfo->data3 = (uintptr_t) callLabel;
      recordInfo->data4 = (uintptr_t) firstInstruction;

      TR::SymbolReference * classSymRef;
      TR_ExternalRelocationTargetKind reloKind;

      if (opCode == TR::New)
         {
         classSymRef = node->getFirstChild()->getSymbolReference();
         reloKind = TR_VerifyClassObjectForAlloc;
         }
      else
         {
         classSymRef = node->getSecondChild()->getSymbolReference();
         reloKind = TR_VerifyRefArrayForAlloc;

         if (comp->getOption(TR_UseSymbolValidationManager))
            classToValidate = comp->fej9()->getComponentClassFromArrayClass(classToValidate);
         }

      if (comp->getOption(TR_UseSymbolValidationManager))
         {
         TR_ASSERT_FATAL(classToValidate, "classToValidate should not be NULL, clazz=%p\n", clazz);
         recordInfo->data5 = (uintptr_t)classToValidate;
         }

      cg->addExternalRelocation(new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(firstInstruction, (uint8_t *) classSymRef, (uint8_t *) recordInfo, reloKind, cg),
               __FILE__, __LINE__, node);
      }

   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

   // At this point the object is initialized and we can move it to a collected register.
   // The out of line path will do the same.
   generateMovInstruction(cg, node, objReg, resultReg, true);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, callReturnLabel);

   // Cleanup registers
   cg->stopUsingRegister(tempReg1);
   cg->stopUsingRegister(tempReg2);
   cg->stopUsingRegister(zeroReg);
   cg->stopUsingRegister(resultReg);
   if (isVariableLength)
      {
      cg->stopUsingRegister(tempReg3);
      }

   cg->decReferenceCount(firstChild);
   if (opCode == TR::New)
      {
      if (classReg != firstChild->getRegister())
         {
         cg->stopUsingRegister(classReg);
         }
      cg->stopUsingRegister(lengthReg);
      }
   else
      {
      cg->decReferenceCount(secondChild);
      if (classReg != secondChild->getRegister())
         {
         cg->stopUsingRegister(classReg);
         }
      }

   node->setRegister(objReg);
   return objReg;
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
   TR::Register *targetRegister = TR::TreeEvaluator::VMnewEvaluator(node, cg);
   if (!targetRegister)
      {
      // Inline object allocation wasn't generated, just generate a call to the helper.
      //
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::acall);
      targetRegister = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      }
   return targetRegister;
   }

TR::Register *
J9::ARM64::TreeEvaluator::newArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = TR::TreeEvaluator::VMnewEvaluator(node, cg);
   if (!targetRegister)
      {
      // Inline array allocation wasn't generated, just generate a call to the helper.
      //
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::acall);
      targetRegister = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      }
   return targetRegister;
   }

TR::Register *
J9::ARM64::TreeEvaluator::anewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = TR::TreeEvaluator::VMnewEvaluator(node, cg);
   if (!targetRegister)
      {
      // Inline array allocation wasn't generated, just generate a call to the helper.
      //
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::acall);
      targetRegister = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      }
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
   generateCompareBranchInstruction(cg, TR::InstOpCode::cbnzx, node, dataReg, incLabel);

   op = fej9->generateCompressedLockWord() ? TR::InstOpCode::stxrw : TR::InstOpCode::stxrx;
   generateTrg1MemSrc1Instruction(cg, op, node, tempReg, new (cg->trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg), metaReg);
   generateCompareBranchInstruction(cg, TR::InstOpCode::cbnzx, node, tempReg, loopLabel);

   generateSynchronizationInstruction(cg, TR::InstOpCode::dmb, node, 0xF); // dmb SY

   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, deps);

   TR::Snippet *snippet = new (cg->trHeapMemory()) TR::ARM64MonitorEnterSnippet(cg, node, incLabel, callLabel, doneLabel);
   cg->addSnippet(snippet);
   doneLabel->setEndInternalControlFlow();

   cg->stopUsingRegister(dataReg);
   cg->stopUsingRegister(addrReg);
   cg->stopUsingRegister(tempReg);

   cg->decReferenceCount(objNode);
   cg->machine()->setLinkRegisterKilled(true);
   return NULL;
   }

TR::Register *
J9::ARM64::TreeEvaluator::arraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   // ldrimmw R1, [B, contiguousSize]
   // cmpimmw R1, 0 ; If 0, must be a discontiguous array
   // ldrimmw R2, [B, discontiguousSize]
   // cselw R1, R1, R2, ne
   //
   TR::Register *objectReg = cg->evaluate(node->getFirstChild());
   TR::Register *lengthReg = cg->allocateRegister();
   TR::Register *discontiguousLengthReg = cg->allocateRegister();

   TR::MemoryReference *contiguousArraySizeMR = new (cg->trHeapMemory()) TR::MemoryReference(objectReg, fej9->getOffsetOfContiguousArraySizeField(), cg);
   TR::MemoryReference *discontiguousArraySizeMR = new (cg->trHeapMemory()) TR::MemoryReference(objectReg, fej9->getOffsetOfDiscontiguousArraySizeField(), cg);

   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmw, node, lengthReg, contiguousArraySizeMR);
   generateCompareImmInstruction(cg, node, lengthReg, 0);
   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmw, node, discontiguousLengthReg, discontiguousArraySizeMR);
   generateCondTrg1Src2Instruction(cg, TR::InstOpCode::cselw, node, lengthReg, lengthReg, discontiguousLengthReg, TR::CC_NE);

   cg->stopUsingRegister(discontiguousLengthReg);
   cg->decReferenceCount(node->getFirstChild());
   node->setRegister(lengthReg);

   return lengthReg;
   }

TR::Register *
J9::ARM64::TreeEvaluator::ZEROCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // NOTE: ZEROCHK is intended to be general and straightforward.  If you're
   // thinking of adding special code for specific situations in here, consider
   // whether you want to add your own CHK opcode instead.  If you feel the
   // need for special handling here, you may also want special handling in the
   // optimizer, in which case a separate opcode may be more suitable.
   //
   // On the other hand, if the improvements you're adding could benefit other
   // users of ZEROCHK, please go ahead and add them!
   //

   TR::LabelSymbol *slowPathLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *restartLabel  = generateLabelSymbol(cg);
   slowPathLabel->setStartInternalControlFlow();
   restartLabel->setEndInternalControlFlow();

   // Temporarily hide the first child so it doesn't appear in the outlined call
   //
   node->rotateChildren(node->getNumChildren()-1, 0);
   node->setNumChildren(node->getNumChildren()-1);

   // Outlined instructions for check failure
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

   // Instructions for the check
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
   // ARM64HelperCallSnippet generates "bl" instruction
   cg->machine()->setLinkRegisterKilled(true);
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

   TR::MemoryReference *storeMR = new (cg->trHeapMemory()) TR::MemoryReference(firstChild, cg);
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
      TR::LabelSymbol *doneLabel, int32_t oldValue, bool oldValueInReg, bool is64bit, bool casWithoutSync = false)
   {
   TR::Register *addrReg = cg->allocateRegister();
   TR::Register *resultReg = cg->allocateRegister();
   TR::InstOpCode::Mnemonic op;

   generateSynchronizationInstruction(cg, TR::InstOpCode::dmb, node, 0xF); // dmb SY

   TR::LabelSymbol *loopLabel = generateLabelSymbol(cg);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel);

   generateTrg1Src2Instruction(cg, TR::InstOpCode::addx, node, addrReg, objReg, offsetReg); // ldxr/stxr instructions does not take offset

   op = is64bit ? TR::InstOpCode::ldxrx : TR::InstOpCode::ldxrw;
   generateTrg1MemInstruction(cg, op, node, resultReg, new (cg->trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg));
   if (oldValueInReg)
      generateCompareInstruction(cg, node, resultReg, oldVReg, is64bit);
   else
      generateCompareImmInstruction(cg, node, resultReg, oldValue, is64bit);
   generateTrg1ImmInstruction(cg, TR::InstOpCode::movzx, node, resultReg, 0); // failure
   generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, doneLabel, TR::CC_NE);

   op = is64bit ? TR::InstOpCode::stxrx : TR::InstOpCode::stxrw;
   generateTrg1MemSrc1Instruction(cg, op, node, resultReg, new (cg->trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg), newVReg);
   generateCompareBranchInstruction(cg, TR::InstOpCode::cbnzx, node, resultReg, loopLabel);

   if (!casWithoutSync)
      generateSynchronizationInstruction(cg, TR::InstOpCode::dmb, node, 0xF); // dmb SY

   generateTrg1ImmInstruction(cg, TR::InstOpCode::movzx, node, resultReg, 1); // success

   cg->stopUsingRegister(addrReg);

   node->setRegister(resultReg);
   return resultReg;
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
   intptr_t oldValue = 0;
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
   resultReg = genCAS(node, cg, objReg, offsetReg, oldVReg, newVReg, doneLabel, oldValue, oldValueInReg, isLong, casWithoutSync);

   conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(5, 5, cg->trMemory());
   TR::addDependency(conditions, objReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, offsetReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, resultReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, newVReg, TR::RealRegister::NoReg, TR_GPR, cg);
   if (oldValueInReg)
      TR::addDependency(conditions, oldVReg, TR::RealRegister::NoReg, TR_GPR, cg);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

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
            if (!methodSymbol->isNative())
               break;

            if ((node->isUnsafeGetPutCASCallOnNonArray() || !TR::Compiler->om.canGenerateArraylets()) && node->isSafeForCGToFastPathUnsafeCall())
               {
               resultReg = VMinlineCompareAndSwap(node, cg, false);
               return true;
               }
            break;
            }

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
   uintptr_t mask = TR::Compiler->om.maskOfObjectVftField();
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
   TR::MemoryReference *mref = new (cg->trHeapMemory()) TR::MemoryReference(node, node->getSymbolReference(), cg);

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
   TR::Instruction *cbzInstruction = generateCompareBranchInstruction(cg, TR::InstOpCode::cbzx, node, referenceReg, snippetLabel);
   cbzInstruction->setNeedsGCMap(0xffffffff);
   snippet->gcMap().setGCRegisterMask(0xffffffff);
   // ARM64HelperCallSnippet generates "bl" instruction
   cg->machine()->setLinkRegisterKilled(true);

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
            cg->decReferenceCount(firstChild);
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
   TR::Register *returnRegister;

   if (!cg->inlineDirectCall(node, returnRegister))
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      TR::MethodSymbol *callee = symRef->getSymbol()->castToMethodSymbol();
      TR::Linkage *linkage;

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
