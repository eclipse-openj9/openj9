/*******************************************************************************
 * Copyright (c) 2019, 2021 IBM Corp. and others
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
#include <iterator>
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
#include "compile/VirtualGuard.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/DataTypes.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/OMRDataTypes_inlines.hpp"
#include "il/StaticSymbol.hpp"
#include "OMR/Bytes.hpp"

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
   tet[TR::BNDCHKwithSpineCHK] = TR::TreeEvaluator::BNDCHKwithSpineCHKEvaluator;
   tet[TR::SpineCHK] = TR::TreeEvaluator::BNDCHKwithSpineCHKEvaluator;
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

   TR::SymbolReference *helperSym = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_softwareReadBarrier);
   generateImmSymInstruction(cg, TR::InstOpCode::bl, node, (uintptr_t)helperSym->getMethodAddress(), deps, helperSym, NULL);

   generateTrg1MemInstruction(cg, loadOp, node, tempReg, new (cg->trHeapMemory()) TR::MemoryReference(locationReg, 0, cg));

   if (isArdbari && node->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef())
      TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, tempReg);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, endLabel, deps);

   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && comp->target().isSMP());
   if (needSync)
      {
      // Issue an Acquire barrier after volatile load
      // dmb ishld
      generateSynchronizationInstruction(cg, TR::InstOpCode::dmb, node, 0x9);
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

/**
 * @brief Generates inlined code for card marking and branch to wrbar helper
 * @details
 *    This method generates code for write barrier for generational GC policies.
 *    It generates inlined code for
 *    - checking whether the destination object is tenured
 *    - checking if concurrent mark thread is active (for gc_modron_wrtbar_cardmark_and_oldcheck)
 *    - card marking (for gc_modron_wrtbar_cardmark_and_oldcheck)
 *    - checking if source object is in new space
 *    - checking if remembered bit is set in object header
 *
 * @param node:       node
 * @param dstReg:     register holding owning object
 * @param srcReg:     register holding source object
 * @param srm:        scratch register manager
 * @param doneLabel:  done label
 * @param wbRef:      symbol reference for write barrier helper
 * @param cg:         code generator
 */
static void
VMnonNullSrcWrtBarCardCheckEvaluator(
      TR::Node *node,
      TR::Register *dstReg,
      TR::Register *srcReg,
      TR_ARM64ScratchRegisterManager *srm,
      TR::LabelSymbol *doneLabel,
      TR::SymbolReference *wbRef ,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   auto gcMode = TR::Compiler->om.writeBarrierType();
   bool doWrtBar = (gcMode == gc_modron_wrtbar_oldcheck || gcMode == gc_modron_wrtbar_cardmark_and_oldcheck || gcMode == gc_modron_wrtbar_always);
   bool doCrdMrk = (gcMode == gc_modron_wrtbar_cardmark_and_oldcheck);
   //We need to do a runtime check on cardmarking for gencon policy if our dstReg is in tenure

   if (gcMode != gc_modron_wrtbar_always)
      {
      /*
       * Generating code checking whether an object is tenured
       *
       * movzx temp1Reg, #heapBase
       * subx  temp1Reg, dstReg, temp1Reg
       * movzx temp2Reg, #heapSize
       * cmpx  temp1Reg, temp2Reg
       * b.cs  doneLabel
       *
       */
      TR::Register *temp1Reg = srm->findOrCreateScratchRegister();
      TR::Register *temp2Reg = srm->findOrCreateScratchRegister();
      TR::Register *metaReg = cg->getMethodMetaDataRegister();

      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:010VMnonNullSrcWrtBarCardCheckEvaluator"), *srm);

      if (comp->getOptions()->isVariableHeapBaseForBarrierRange0() || comp->compileRelocatableCode())
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, temp1Reg, new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapBaseForBarrierRange0), cg));
         }
      else
         {
         uintptr_t heapBase = comp->getOptions()->getHeapBaseForBarrierRange0();
         loadAddressConstant(cg, node, heapBase, temp1Reg);
         }
      generateTrg1Src2Instruction(cg, TR::InstOpCode::subx, node, temp1Reg, dstReg, temp1Reg);

      if (comp->getOptions()->isVariableHeapSizeForBarrierRange0())
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, temp2Reg, new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapSizeForBarrierRange0), cg));
         }
      else
         {
         uintptr_t heapSize = comp->getOptions()->getHeapSizeForBarrierRange0();
         loadConstant64(cg, node, heapSize, temp2Reg);
         }
      generateCompareInstruction(cg, node, temp1Reg, temp2Reg, true);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, doneLabel, TR::CC_CS);
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:010VMnonNullSrcWrtBarCardCheckEvaluator:01oldCheckDone"), *srm);

      TR::LabelSymbol *noChkLabel = generateLabelSymbol(cg);
      if (doCrdMrk)
         {
         /*
          * Check if J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE flag is set.
          * If not, skip card dirtying.
          *
          *   ldrimmx temp2Reg, [vmThread, #privateFlag]
          *   tbz     temp2Reg, #20, crdMrkDoneLabel
          *   ldrimmx temp2Reg, [vmThread, #activeCardTableBase]
          *   addx    temp2Reg, temp2Reg, temp1Reg, LSR #card_size_shift ; At this moment, temp1Reg contains (dstReg - #heapBase)
          *   movzx   temp1Reg, 1
          *   strbimm temp1Reg, [temp2Reg, 0]
          *
          * crdMrkDoneLabel:
          */
         cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:010VMnonNullSrcWrtBarCardCheckEvaluator:02cardmark"), *srm);

         static_assert(J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE == (1 << 20), "We assume that J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE is 0x100000");
         TR::LabelSymbol *crdMrkDoneLabel = generateLabelSymbol(cg);
         generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, temp2Reg, new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, privateFlags), cg));
         generateTestBitBranchInstruction(cg, TR::InstOpCode::tbz, node, temp2Reg, 20, crdMrkDoneLabel);

         cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:010VMnonNullSrcWrtBarCardCheckEvaluator:03markThreadActiveCheckDone"), *srm);

         uintptr_t card_size_shift = trailingZeroes((uint64_t)comp->getOptions()->getGcCardSize());
         if (comp->getOptions()->isVariableActiveCardTableBase() || comp->compileRelocatableCode())
            {
            generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, temp2Reg, new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, activeCardTableBase), cg));
            }
         else
            {
            uintptr_t activeCardTableBase = comp->getOptions()->getActiveCardTableBase();
            loadAddressConstant(cg, node, activeCardTableBase, temp2Reg);
            }
         generateTrg1Src2ShiftedInstruction(cg, TR::InstOpCode::addx, node, temp2Reg, temp2Reg, temp1Reg, TR::SH_LSR, card_size_shift);
         generateTrg1ImmInstruction(cg, TR::InstOpCode::movzx, node, temp1Reg, 1);
         generateMemSrc1Instruction(cg, TR::InstOpCode::strbimm, node, new (cg->trHeapMemory()) TR::MemoryReference(temp2Reg, 0, cg), temp1Reg);

         generateLabelInstruction(cg, TR::InstOpCode::label, node, crdMrkDoneLabel);

         cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:010VMnonNullSrcWrtBarCardCheckEvaluator:04cardmarkDone"), *srm);
         }

      /*
       * Generating code checking whether the src is in new space
       *
       *   movzx temp1Reg, #heapBase
       *   subx  temp1Reg, srcReg, temp1Reg
       *   movzx temp2Reg, #heapSize
       *   cmpx  temp1Reg, temp2Reg
       *   b.cc  doneLabel
       */
      if (comp->getOptions()->isVariableHeapBaseForBarrierRange0() || comp->compileRelocatableCode())
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, temp1Reg, new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapBaseForBarrierRange0), cg));
         }
      else
         {
         uintptr_t heapBase = comp->getOptions()->getHeapBaseForBarrierRange0();
         loadAddressConstant(cg, node, heapBase, temp1Reg);
         }
      generateTrg1Src2Instruction(cg, TR::InstOpCode::subx, node, temp1Reg, srcReg, temp1Reg);

      // If doCrdMrk is false, then temp2Reg still contains heapSize
      if (doCrdMrk)
         {
         if (comp->getOptions()->isVariableHeapSizeForBarrierRange0())
            {
            generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, temp2Reg, new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapSizeForBarrierRange0), cg));
            }
         else
            {
            uintptr_t heapSize = comp->getOptions()->getHeapSizeForBarrierRange0();
            loadConstant64(cg, node, heapSize, temp2Reg);
            }
         }

      generateCompareInstruction(cg, node, temp1Reg, temp2Reg, true);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, doneLabel, TR::CC_CC);

      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:010VMnonNullSrcWrtBarCardCheckEvaluator:05sourceCheckDone"), *srm);

      /*
       * Generating code checking whether the remembered bit is set
       *
       *   ldrimmx temp1Reg, [dstReg, #offsetOfHeaderFlags]
       *   tstimmw temp1Reg, J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST
       *   b.ne    doneLabel
       *   bl      jitWriteBarrierGenerational
       */
      static_assert(J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST == 0xf0, "We assume that J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST is 0xf0");
      generateTrg1MemInstruction(cg, (TR::Compiler->om.compressObjectReferences() ? TR::InstOpCode::ldrimmw : TR::InstOpCode::ldrimmx), node, temp1Reg, new (cg->trHeapMemory()) TR::MemoryReference(dstReg, TR::Compiler->om.offsetOfHeaderFlags(), cg));
      generateTestImmInstruction(cg, node, temp1Reg, 0x703, false); // 0x703 is immr:imms for 0xf0
      generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, doneLabel, TR::CC_NE);

      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:010VMnonNullSrcWrtBarCardCheckEvaluator:06rememberedBitCheckDone"), *srm);

      srm->reclaimScratchRegister(temp1Reg);
      srm->reclaimScratchRegister(temp2Reg);
      }
   generateImmSymInstruction(cg, TR::InstOpCode::bl, node, reinterpret_cast<uintptr_t>(wbRef->getMethodAddress()), NULL, wbRef, NULL);
   cg->machine()->setLinkRegisterKilled(true);
   }

/**
 * @brief Generates inlined code for card marking
 * @details
 *    This method generates code for write barrier for optavgpause/balanced GC policies.
 *    It generates inlined code for
 *    - checking if concurrent mark thread is active (for optavgpause)
 *    - checking whether the destination object is in heap
 *    - card marking
 *
 * @param node:       node
 * @param dstReg:     register holding owning object
 * @param srm:        scratch register manager
 * @param doneLabel:  done label
 * @param cg:         code generator
 */
static void
VMCardCheckEvaluator(
      TR::Node *node,
      TR::Register *dstReg,
      TR_ARM64ScratchRegisterManager *srm,
      TR::LabelSymbol *doneLabel,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   auto gcMode = TR::Compiler->om.writeBarrierType();
   TR::Register *temp1Reg = srm->findOrCreateScratchRegister();
   TR::Register *metaReg = cg->getMethodMetaDataRegister();

   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:020VMCardCheckEvaluator"), *srm);
   // If gcpolicy is balanced, we must always do card marking
   if (gcMode != gc_modron_wrtbar_cardmark_incremental)
      {
      /*
       * Check if J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE flag is set.
       * If not, skip card dirtying.
       *
       *   ldrimmx temp1Reg, [vmThread, #privateFlag]
       *   tbz     temp1Reg, #20, doneLabel
       */

      static_assert(J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE == (1 << 20), "We assume that J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE is 0x100000");
      generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, temp1Reg, new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, privateFlags), cg));
      generateTestBitBranchInstruction(cg, TR::InstOpCode::tbz, node, temp1Reg, 20, doneLabel);

      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:020VMCardCheckEvaluator:01markThreadActiveCheckDone"), *srm);
      }

   TR::Register *temp2Reg = srm->findOrCreateScratchRegister();
      /*
       * Generating code checking whether an object is in heap
       *
       * movzx temp1Reg, #heapBase
       * subx  temp1Reg, dstReg, temp1Reg
       * movzx temp2Reg, #heapSize
       * cmpx  temp1Reg, temp2Reg
       * b.cs  doneLabel
       *
       */

   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:020VMCardCheckEvaluator:020heapCheck"), *srm);

   if (comp->getOptions()->isVariableHeapBaseForBarrierRange0() || comp->compileRelocatableCode())
      {
      generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, temp1Reg, new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapBaseForBarrierRange0), cg));
      }
   else
      {
      uintptr_t heapBase = comp->getOptions()->getHeapBaseForBarrierRange0();
      loadAddressConstant(cg, node, heapBase, temp1Reg);
      }
   generateTrg1Src2Instruction(cg, TR::InstOpCode::subx, node, temp1Reg, dstReg, temp1Reg);

   // If we know the object is definitely in heap, then we skip the check.
   if (!node->isHeapObjectWrtBar())
      {
      if (comp->getOptions()->isVariableHeapSizeForBarrierRange0())
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, temp2Reg, new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapSizeForBarrierRange0), cg));
         }
      else
         {
         uintptr_t heapSize = comp->getOptions()->getHeapSizeForBarrierRange0();
         loadConstant64(cg, node, heapSize, temp2Reg);
         }
      generateCompareInstruction(cg, node, temp1Reg, temp2Reg, true);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, doneLabel, TR::CC_CS);
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:020VMCardCheckEvaluator:03heapCheckDone"), *srm);
      }

   /*
    *  Generating card dirtying sequence.
    *  We don't call out to VM helpers.
    *
    *  ldrimmx temp2Reg, [vmThread, #activeCardTableBase]
    *  addx    temp2Reg, temp2Reg, temp1Reg, LSR #card_size_shift ; At this moment, temp1Reg contains (dstReg - #heapBase)
    *  movzx   temp1Reg, 1
    *  strbimm temp1Reg, [temp2Reg, 0]
    *
    */
   uintptr_t card_size_shift = trailingZeroes((uint64_t)comp->getOptions()->getGcCardSize());
   if (comp->getOptions()->isVariableActiveCardTableBase() || comp->compileRelocatableCode())
      {
      generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, temp2Reg, new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, activeCardTableBase), cg));
      }
   else
      {
      uintptr_t activeCardTableBase = comp->getOptions()->getActiveCardTableBase();
      loadAddressConstant(cg, node, activeCardTableBase, temp2Reg);
      }
   generateTrg1Src2ShiftedInstruction(cg, TR::InstOpCode::addx, node, temp2Reg, temp2Reg, temp1Reg, TR::SH_LSR, card_size_shift);
   generateTrg1ImmInstruction(cg, TR::InstOpCode::movzx, node, temp1Reg, 1);
   generateMemSrc1Instruction(cg, TR::InstOpCode::strbimm, node, new (cg->trHeapMemory()) TR::MemoryReference(temp2Reg, 0, cg), temp1Reg);

   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:020VMCardCheckEvaluator:04cardmarkDone"), *srm);
   }

static void wrtbarEvaluator(TR::Node *node, TR::Register *srcReg, TR::Register *dstReg, bool srcNonNull, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Instruction * cursor;
   auto gcMode = TR::Compiler->om.writeBarrierType();
   bool doWrtBar = (gcMode == gc_modron_wrtbar_oldcheck || gcMode == gc_modron_wrtbar_cardmark_and_oldcheck || gcMode == gc_modron_wrtbar_always);
   bool doCrdMrk = (gcMode == gc_modron_wrtbar_cardmark ||gcMode == gc_modron_wrtbar_cardmark_and_oldcheck || gcMode == gc_modron_wrtbar_cardmark_incremental);

   if ((node->getOpCode().isWrtBar() && node->skipWrtBar()) || node->isNonHeapObjectWrtBar())
      return;

   TR_ARM64ScratchRegisterManager *srm = cg->generateScratchRegisterManager();
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);

   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator"), *srm);

   if (doWrtBar) // generational or gencon
      {
      TR::SymbolReference *wbRef = (gcMode == gc_modron_wrtbar_always) ?
         comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef() :
         // use jitWriteBarrierStoreGenerational for both generational and gencon, because we inline card marking.
         comp->getSymRefTab()->findOrCreateWriteBarrierStoreGenerationalSymbolRef();

      if (!srcNonNull)
         {
         // If object is NULL, done
         cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:000srcNullChk"), *srm);
         generateCompareBranchInstruction(cg, TR::InstOpCode::cbzx, node, srcReg, doneLabel);
         cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:000srcNullChk:NonNull"), *srm);
         }
      // Inlines cardmarking and remembered bit check for gencon.
      VMnonNullSrcWrtBarCardCheckEvaluator(node, dstReg, srcReg, srm, doneLabel, wbRef, cg);

      }
   else if (doCrdMrk)
      {
      TR::SymbolReference *wbRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef();
      if (!srcNonNull)
         {
         cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:000srcNullChk"), *srm);
         generateCompareBranchInstruction(cg, TR::InstOpCode::cbzx, node, srcReg, doneLabel);
         cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:000srcNullChk:NonNull"), *srm);
         }
      VMCardCheckEvaluator(node, dstReg, srm, doneLabel, cg);
      }

   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2 + srm->numAvailableRegisters(), cg->trMemory());
   conditions->addPostCondition(dstReg, doWrtBar ? TR::RealRegister::x0 : TR::RealRegister::NoReg);
   conditions->addPostCondition(srcReg, doWrtBar ? TR::RealRegister::x1 : TR::RealRegister::NoReg);
   srm->addScratchRegistersToDependencyList(conditions);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions, NULL);

   srm->stopUsingRegisters();
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
   bool isVolatileMode = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());
   bool isOrderedMode = (node->getSymbolReference()->getSymbol()->isShadow() && node->getSymbolReference()->getSymbol()->isOrdered() && cg->comp()->target().isSMP());

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

   // Issue a StoreStore barrier before each volatile store.
   // dmb ishst
   if (isVolatileMode || isOrderedMode)
      generateSynchronizationInstruction(cg, TR::InstOpCode::dmb, node, 0xA);

   generateMemSrc1Instruction(cg, TR::InstOpCode::strimmx, node, tempMR, sourceRegister, NULL);

   // Issue a StoreLoad barrier after each volatile store.
   // dmb ish
   if (isVolatileMode)
      generateSynchronizationInstruction(cg, TR::InstOpCode::dmb, node, 0xB);

   wrtbarEvaluator(node, sourceRegister, destinationRegister, firstChild->isNonNull(), cg);

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
   bool isVolatileMode = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());
   bool isOrderedMode = (node->getSymbolReference()->getSymbol()->isShadow() && node->getSymbolReference()->getSymbol()->isOrdered() && cg->comp()->target().isSMP());

   if (comp->useCompressedPointers() && (node->getSymbolReference()->getSymbol()->getDataType() == TR::Address) && (node->getSecondChild()->getDataType() != TR::Address))
      {
      usingCompressedPointers = true;

      while (secondChild->getNumChildren() && secondChild->getOpCodeValue() != TR::a2l)
         secondChild = secondChild->getFirstChild();
      if (secondChild->getNumChildren())
         secondChild = secondChild->getFirstChild();
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

   // Issue a StoreStore barrier before each volatile store.
   // dmb ishst
   if (isVolatileMode || isOrderedMode)
      generateSynchronizationInstruction(cg, TR::InstOpCode::dmb, node, 0xA);

   generateMemSrc1Instruction(cg, storeOp, node, tempMR, translatedSrcReg);

   // Issue a StoreLoad barrier after each volatile store.
   // dmb ish
   if (isVolatileMode)
      generateSynchronizationInstruction(cg, TR::InstOpCode::dmb, node, 0xB);

   wrtbarEvaluator(node, sourceRegister, destinationRegister, secondChild->isNonNull(), cg);

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

/**
 * @brief Generates instructions to load j9class from object pointer
 *
 * @param[in]       node: node
 * @param[in] j9classReg: register j9class value is assigned to
 * @param[in]     objReg: register holding object pointer
 * @param[in]         cg: code generator
 */
static void
generateLoadJ9Class(TR::Node *node, TR::Register *j9classReg, TR::Register *objReg, TR::CodeGenerator *cg)
   {
   generateTrg1MemInstruction(cg, TR::Compiler->om.compressObjectReferences() ? TR::InstOpCode::ldrimmw : TR::InstOpCode::ldrimmx, node, j9classReg,
      new (cg->trHeapMemory()) TR::MemoryReference(objReg, static_cast<int32_t>(TR::Compiler->om.offsetOfObjectVftField()), cg));
   TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, j9classReg);
   }

void
J9::ARM64::TreeEvaluator::generateCheckForValueMonitorEnterOrExit(TR::Node *node, TR::LabelSymbol *helperCallLabel, TR::Register *objReg, TR::Register *temp1Reg, TR::Register *temp2Reg, TR::CodeGenerator *cg, int32_t classFlag)
{
   // get class of object
   generateLoadJ9Class(node, temp1Reg, objReg, cg);

   // get memory reference to class flags
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR::MemoryReference *classFlagsMemRef = new (cg->trHeapMemory()) TR::MemoryReference(temp1Reg, static_cast<uintptr_t>(fej9->getOffsetOfClassFlags()), cg);

   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmw, node, temp1Reg, classFlagsMemRef);
   loadConstant32(cg, node, classFlag, temp2Reg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::andw, node, temp1Reg, temp1Reg, temp2Reg);

   // If obj is value type or value based class instance, call VM helper and throw IllegalMonitorState exception, else continue as usual
   generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, helperCallLabel, TR::CC_NE);
}

TR::Register *
J9::ARM64::TreeEvaluator::monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = TR::comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   int32_t lwOffset = fej9->getByteOffsetToLockword(cg->getMonClass(node));
   TR::InstOpCode::Mnemonic op;
   TR_YesNoMaybe isMonitorValueBasedOrValueType = cg->isMonitorValueBasedOrValueType(node);

   if (comp->getOption(TR_FullSpeedDebug) ||
       (isMonitorValueBasedOrValueType == TR_yes) ||
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
   TR::Register *zeroReg = cg->allocateRegister();
   TR::Register *metaReg = cg->getMethodMetaDataRegister();

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(5, 5, cg->trMemory());
   TR::addDependency(deps, objReg, TR::RealRegister::x0, TR_GPR, cg);
   /* We need following 3 registers at index 1-3 of regdeps as ARM64MonitorExitSnippet expects it. */
   TR::addDependency(deps, dataReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(deps, addrReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(deps, tempReg, TR::RealRegister::NoReg, TR_GPR, cg);

   TR::addDependency(deps, zeroReg, TR::RealRegister::xzr, TR_GPR, cg);

   TR::LabelSymbol *callLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *decLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   // If object is not known to be value type or value based class at compile time, check at run time
   if (isMonitorValueBasedOrValueType == TR_maybe)
      {
      generateCheckForValueMonitorEnterOrExit(node, callLabel, objReg, tempReg, dataReg, cg, J9_CLASS_DISALLOWS_LOCKING_FLAGS);
      }

   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addimmx, node, addrReg, objReg, lwOffset);
   op = fej9->generateCompressedLockWord() ? TR::InstOpCode::ldrimmw : TR::InstOpCode::ldrimmx;
   auto faultingInstruction = generateTrg1MemInstruction(cg, op, node, dataReg, new (cg->trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg));

   // InstructonDelegate::setupImplicitNullPointerException checks if the memory reference uses nullcheck reference register.
   // In this case, nullcheck reference register is objReg, but the memory reference does not use it,
   // thus we need to explicitly set implicit exception point here.
   if (cg->getHasResumableTrapHandler() && cg->getCurrentEvaluationTreeTop()->getNode()->getOpCode().isNullCheck())
      {
      if (cg->getImplicitExceptionPoint() == NULL)
         {
         if (comp->getOption(TR_TraceCG))
            {
            traceMsg(comp, "Instruction %p throws an implicit NPE, node: %p NPE node: %p\n", faultingInstruction, node, objNode);
            }
         cg->setImplicitExceptionPoint(faultingInstruction);
         }
      }

   generateCompareInstruction(cg, node, dataReg, metaReg, true);

   generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, decLabel, TR::CC_NE);

   static const bool useMemoryBarrierForMonitorExit = feGetEnv("TR_aarch64UseMemoryBarrierForMonitorExit") != NULL;
   if (useMemoryBarrierForMonitorExit)
      {
      generateSynchronizationInstruction(cg, TR::InstOpCode::dmb, node, 0xB); // dmb ish (Inner Shareable full barrier)
      op = fej9->generateCompressedLockWord() ? TR::InstOpCode::strimmw : TR::InstOpCode::strimmx;
      }
   else
      {
      op = fej9->generateCompressedLockWord() ? TR::InstOpCode::stlrw : TR::InstOpCode::stlrx;
      }

   generateMemSrc1Instruction(cg, op, node, new (cg->trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg), zeroReg);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, deps);

   TR::Snippet *snippet = new (cg->trHeapMemory()) TR::ARM64MonitorExitSnippet(cg, node, decLabel, callLabel, doneLabel);
   cg->addSnippet(snippet);
   doneLabel->setEndInternalControlFlow();

   cg->stopUsingRegister(dataReg);
   cg->stopUsingRegister(addrReg);
   cg->stopUsingRegister(tempReg);
   cg->stopUsingRegister(zeroReg);

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
   return VMinstanceofEvaluator(node, cg);
   }

/**
 *  @brief Generates Superclass Test for checkcast/instanceof/ArrayStoreCHK nodes.
 *  @details
 *    It will generate pseudocode as follows.
 *    if (objectClassDepth <= castClassDepth) call Helper
 *    else
 *    load superClassArrReg,superClassOfObjectClass
 *    cmp superClassArrReg[castClassDepth], castClass
 *    Here It sets up the condition code for callee to react on.
 *
 *  @param[in] node:                           node
 *  @param[in] instanceClassReg:               register contains instance class
 *  @param[in] instanceClassRegCanBeReclaimed: if true, instanceClassReg is reclaimed
 *  @param[in] castClassReg:                   register contains cast class
 *  @param[in] castClassDepth:                 class depth of the cast class. If -1 is passed, depth is loaded at runtime
 *  @param[in] falseLabel:                     label to jump when test fails
 *  @param[in] srm:                            scratch register manager
 *  @param[in] cg:                             code generator
 */
static
void genSuperClassTest(TR::Node *node, TR::Register *instanceClassReg, bool instanceClassRegCanBeReclaimed, TR::Register *castClassReg, int32_t castClassDepth,
                                            TR::LabelSymbol *falseLabel, TR_ARM64ScratchRegisterManager *srm, TR::CodeGenerator *cg)
   {
   // Compare the instance class depth to the cast class depth. If the instance class depth is less than or equal to
   // to the cast class depth then the cast class cannot be a superclass of the instance class.
   //
   TR::Register *instanceClassDepthReg = srm->findOrCreateScratchRegister();
   TR::Register *castClassDepthReg = NULL;
   static_assert(J9AccClassDepthMask == 0xffff, "J9_JAVA_CLASS_DEPTH_MASK must be 0xffff");
   // load lower 16bit of classDepthAndFlags
   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrhimm, node, instanceClassDepthReg,
                              new (cg->trHeapMemory()) TR::MemoryReference(instanceClassReg, offsetof(J9Class, classDepthAndFlags), cg));
   if (castClassDepth != -1)
      {
      // castClassDepth is known at compile time
      if (constantIsUnsignedImm12(castClassDepth))
         {
         generateCompareImmInstruction(cg, node, instanceClassDepthReg, castClassDepth);
         }
      else
         {
         castClassDepthReg = srm->findOrCreateScratchRegister();
         loadConstant32(cg, node, castClassDepth, castClassDepthReg);
         generateCompareInstruction(cg, node, instanceClassDepthReg, castClassDepthReg);
         }
      }
   else
      {
      // castClassDepth needs to be loaded from castClass
      castClassDepthReg = srm->findOrCreateScratchRegister();
      // load lower 16bit of classDepthAndFlags
      generateTrg1MemInstruction(cg, TR::InstOpCode::ldrhimm, node, castClassDepthReg,
                              new (cg->trHeapMemory()) TR::MemoryReference(castClassReg, offsetof(J9Class, classDepthAndFlags), cg));
      generateCompareInstruction(cg, node, instanceClassDepthReg, castClassDepthReg);
      }
   srm->reclaimScratchRegister(instanceClassDepthReg);
   instanceClassDepthReg = NULL; // prevent re-using this register by error

   // if objectClassDepth is less than or equal to castClassDepth, then call Helper
   generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, falseLabel, TR::CC_LE);

   // Load the superclasses array of the instance class and check if the superclass that appears at the depth of the cast class is in fact the cast class.
   // If not, the instance class and cast class are not in the same hierarchy.
   //
   TR::Register *instanceClassSuperClassesArrayReg = srm->findOrCreateScratchRegister();

   generateTrg1MemInstruction(cg,TR::InstOpCode::ldrimmx, node, instanceClassSuperClassesArrayReg,
                              new (cg->trHeapMemory()) TR::MemoryReference(instanceClassReg, offsetof(J9Class, superclasses), cg));

   if (instanceClassRegCanBeReclaimed)
      {
      srm->reclaimScratchRegister(instanceClassReg);
      instanceClassReg = NULL; // prevent re-using this register by error
      }

   TR::Register *instanceClassSuperClassReg = srm->findOrCreateScratchRegister();

   int32_t castClassDepthOffset = castClassDepth * TR::Compiler->om.sizeofReferenceAddress();
   if ((castClassDepth != -1) && constantIsUnsignedImm12(castClassDepthOffset))
      {
      generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, instanceClassSuperClassReg,
                                 new (cg->trHeapMemory()) TR::MemoryReference(instanceClassSuperClassesArrayReg, castClassDepthOffset, cg));
      }
   else
      {
      if (!castClassDepthReg)
         {
         castClassDepthReg = srm->findOrCreateScratchRegister();
         loadConstant32(cg, node, castClassDepth, castClassDepthReg);
         }
      generateLogicalShiftLeftImmInstruction(cg, node, castClassDepthReg, castClassDepthReg, 3, false);
      generateTrg1MemInstruction(cg, TR::InstOpCode::ldroffx, node, instanceClassSuperClassReg, new (cg->trHeapMemory()) TR::MemoryReference(instanceClassSuperClassesArrayReg, castClassDepthReg, cg));
      }
   generateCompareInstruction(cg, node, instanceClassSuperClassReg, castClassReg, true);

   if (castClassDepthReg)
      srm->reclaimScratchRegister(castClassDepthReg);
   srm->reclaimScratchRegister(instanceClassSuperClassesArrayReg);
   srm->reclaimScratchRegister(instanceClassSuperClassReg);

   // At this point EQ flag will be set if the cast class is a superclass of the instance class. Caller is responsible for acting on the result.
   }

/** 
 * @brief Generates Arbitrary Class Test for instanceOf or checkCast node
 */
static
void genInstanceOfOrCheckCastArbitraryClassTest(TR::Node *node, TR::Register *instanceClassReg, TR_OpaqueClassBlock *arbitraryClass,
                                                TR_ARM64ScratchRegisterManager *srm, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Register *arbitraryClassReg = srm->findOrCreateScratchRegister();
   TR_J9VMBase *fej9 = static_cast<TR_J9VMBase *>(comp->fe());

   if (comp->compileRelocatableCode())
      {
      loadAddressConstantInSnippet(cg, node, reinterpret_cast<intptr_t>(arbitraryClass), arbitraryClassReg, TR_ClassPointer);
      }
   else
      {
      bool isUnloadAssumptionRequired = fej9->isUnloadAssumptionRequired(arbitraryClass, comp->getCurrentMethod());

      if (isUnloadAssumptionRequired)
         {
         loadAddressConstantInSnippet(cg, node, reinterpret_cast<intptr_t>(arbitraryClass), arbitraryClassReg, TR_NoRelocation, true); 
         }
      else
         {
         loadAddressConstant(cg, node, reinterpret_cast<intptr_t>(arbitraryClass), arbitraryClassReg, NULL, true);
         }
      }
   generateCompareInstruction(cg, node, instanceClassReg, arbitraryClassReg, true);
   
   srm->reclaimScratchRegister(arbitraryClassReg);

   // At this point EQ flag will be set if the cast class matches the arbitrary class. Caller is responsible for acting on the result.
   }

/** 
 * @brief Generates ArrayOfJavaLangObjectTest (object class is reference array) for instanceOf or checkCast node
 * @details
 *    scratchReg1 = load (objectClassReg+offset_romClass)
 *    scratchReg1 = load (ROMClass+J9ROMClass+modifiers)
 *    tstImmediate with J9AccClassArray(0x10000)
 *    If not Array -> Branch to Fail Label
 *    testerReg = load (objectClassReg + leafcomponent_offset)
 *    testerReg = load (objectClassReg + offset_romClass)
 *    testerReg = load (objectClassReg + offset_modifiers)
 *    tstImmediate with J9AccClassInternalPrimitiveType(0x20000)
 *    // if branchOnPrimitiveTypeCheck is true
 *    If arrays of primitive -> Branch to Fail Label
 *    // else
 *    if not arrays of primitive set condition code to Zero indicating true result
 */
static
void genInstanceOfOrCheckCastObjectArrayTest(TR::Node *node, TR::Register *instanceClassReg, TR::LabelSymbol *falseLabel, bool useTBZ,
                                             TR_ARM64ScratchRegisterManager *srm, TR::CodeGenerator *cg)
   {
   // Load the object ROM class and test the modifiers to see if this is an array.
   //
   TR::Register *scratchReg = srm->findOrCreateScratchRegister();
   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, scratchReg, new (cg->trHeapMemory()) TR::MemoryReference(instanceClassReg, offsetof(J9Class, romClass), cg));
   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmw, node, scratchReg, new (cg->trHeapMemory()) TR::MemoryReference(scratchReg, offsetof(J9ROMClass, modifiers), cg));
   static_assert(J9AccClassArray == 0x10000, "J9AccClassArray must be 0x10000");
   // If not array, branch to falseLabel
   if (useTBZ)
      {
      generateTestBitBranchInstruction(cg, TR::InstOpCode::tbz, node, scratchReg, 16, falseLabel);
      }
   else
      {
      generateTestImmInstruction(cg, node, scratchReg, 0x400); // 0x400 is immr:imms for 0x10000
      generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, falseLabel, TR::CC_EQ);
      }

   // If it's an array, load the component ROM class and test the modifiers to see if this is a primitive array.
   //
   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, scratchReg, new (cg->trHeapMemory()) TR::MemoryReference(instanceClassReg, offsetof(J9ArrayClass, componentType), cg));
   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, scratchReg, new (cg->trHeapMemory()) TR::MemoryReference(scratchReg, offsetof(J9Class, romClass), cg));
   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmw, node, scratchReg, new (cg->trHeapMemory()) TR::MemoryReference(scratchReg, offsetof(J9ROMClass, modifiers), cg));

   static_assert(J9AccClassInternalPrimitiveType == 0x20000, "J9AccClassInternalPrimitiveType must be 0x20000");
   generateTestImmInstruction(cg, node, scratchReg, 0x3c0); // 0x3c0 is immr:imms for 0x20000

   srm->reclaimScratchRegister(scratchReg);

   // At this point EQ flag will be set if this is not a primitive array. Caller is responsible acting on the result.
   }

template<class It>
bool
isTerminalSequence(It it, It itEnd)
   {
   return (it + 1) == itEnd;
   }

template<class It>
bool
isNextItemGoToTrue(It it, It itEnd)
   {
   return (!isTerminalSequence(it, itEnd)) && *(it + 1) == J9::TreeEvaluator::GoToTrue;
   }

template<class It>
bool
isNextItemGoToFalse(It it, It itEnd)
   {
   return (!isTerminalSequence(it, itEnd)) && *(it + 1) == J9::TreeEvaluator::GoToFalse;
   }

template<class It>
bool
isNextItemHelperCall(It it, It itEnd)
   {
   return (!isTerminalSequence(it, itEnd)) && *(it + 1) == J9::TreeEvaluator::HelperCall;
   }

TR::Register *
J9::ARM64::TreeEvaluator::VMinstanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation                      *comp = cg->comp();
   TR_OpaqueClassBlock                  *compileTimeGuessClass;
   int32_t                               maxProfiledClasses = comp->getOptions()->getCheckcastMaxProfiledClassTests();
   traceMsg(comp, "%s:Maximum Profiled Classes = %d\n", node->getOpCode().getName(),maxProfiledClasses);
   TR_ASSERT_FATAL(maxProfiledClasses <= 4, "Maximum 4 profiled classes per site allowed because we use a fixed stack allocated buffer for profiled classes\n");
   InstanceOfOrCheckCastSequences        sequences[InstanceOfOrCheckCastMaxSequences];
   bool                                  topClassWasCastClass = false;
   float                                 topClassProbability = 0.0;

   bool                                  profiledClassIsInstanceOf;
   InstanceOfOrCheckCastProfiledClasses  profiledClassesList[4];
   uint32_t                              numberOfProfiledClass;
   uint32_t                              numSequencesRemaining = calculateInstanceOfOrCheckCastSequences(node, sequences, &compileTimeGuessClass, cg, profiledClassesList, &numberOfProfiledClass, maxProfiledClasses, &topClassProbability, &topClassWasCastClass);


   TR::Node                       *objectNode = node->getFirstChild();
   TR::Node                       *castClassNode = node->getSecondChild();
   TR::Register                   *objectReg = cg->evaluate(objectNode);
   TR::Register                   *castClassReg = NULL;
   TR::Register                   *resultReg = cg->allocateRegister();

   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *callHelperLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *nextSequenceLabel = generateLabelSymbol(cg);

   TR::Instruction *gcPoint;

   TR_ARM64ScratchRegisterManager *srm = cg->generateScratchRegisterManager();
   TR::Register                 *objectClassReg = NULL;

   // initial result is false
   generateTrg1ImmInstruction(cg, TR::InstOpCode::movzx, node, resultReg, 0);

   auto itBegin = std::begin(sequences);
   const auto itEnd = std::next(itBegin, numSequencesRemaining);
   
   for (auto it = itBegin; it != itEnd; it++)
      {
      auto current = *it;
      switch (current)
         {
         case EvaluateCastClass:
            TR_ASSERT(!castClassReg, "Cast class already evaluated");
            castClassReg = cg->gprClobberEvaluate(castClassNode);
            break;
         case LoadObjectClass:
            TR_ASSERT(!objectClassReg, "Object class already loaded");
            objectClassReg = srm->findOrCreateScratchRegister();
            generateLoadJ9Class(node, objectClassReg, objectReg, cg);
            break;
         case NullTest:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting NullTest\n", node->getOpCode().getName());
            TR_ASSERT(!objectNode->isNonNull(), "Object is known to be non-null, no need for a null test");
            if (isNextItemGoToTrue(it, itEnd))
               {
               generateCompareImmInstruction(cg, node, objectReg, 0, true);
               generateCSetInstruction(cg, node, resultReg, TR::CC_NE);
               // consume GoToTrue
               it++;
               }
            else
               {
               auto nullLabel = isNextItemHelperCall(it, itEnd) ? callHelperLabel : doneLabel;
               // branch to doneLabel to return false
               generateCompareBranchInstruction(cg, TR::InstOpCode::cbzx, node, objectReg, nullLabel);
               }
            break;
         case GoToTrue:
            TR_ASSERT_FATAL(isTerminalSequence(it, itEnd), "GoToTrue should be the terminal sequence");
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting GoToTrue\n", node->getOpCode().getName());
            generateTrg1ImmInstruction(cg, TR::InstOpCode::movzx, node, resultReg, 1);
            break;
         case GoToFalse:
            TR_ASSERT_FATAL(isTerminalSequence(it, itEnd), "GoToFalse should be the terminal sequence");
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting GoToFalse\n", node->getOpCode().getName());
            break;
         case ClassEqualityTest:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting ClassEqualityTest\n", node->getOpCode().getName());
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/Equality", comp->signature()),1,TR::DebugCounter::Undetermined);

            generateCompareInstruction(cg, node, objectClassReg, castClassReg, true);
            generateCSetInstruction(cg, node, resultReg, TR::CC_EQ);
            break;
         case SuperClassTest:
            {
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting SuperClassTest\n", node->getOpCode().getName());
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/SuperClassTest", comp->signature()),1,TR::DebugCounter::Undetermined);

            int32_t castClassDepth = castClassNode->getSymbolReference()->classDepth(comp);
            auto falseLabel = isNextItemGoToFalse(it, itEnd) ? doneLabel : (isNextItemHelperCall(it, itEnd) ? callHelperLabel : nextSequenceLabel);
            genSuperClassTest(node, objectClassReg, false, castClassReg, castClassDepth, falseLabel, srm, cg);
            generateCSetInstruction(cg, node, resultReg, TR::CC_EQ);
            }
            break;
         case ProfiledClassTest:
            {
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting ProfiledClassTest\n", node->getOpCode().getName());
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/Profile", comp->signature()),1,TR::DebugCounter::Undetermined);

            auto profiledClassesIt = std::begin(profiledClassesList);
            auto profiledClassesItEnd = std::next(profiledClassesIt, numberOfProfiledClass);
            while (profiledClassesIt != profiledClassesItEnd)
               {
               if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: ProfiledClassTest: profiledClass = %p, isProfiledClassInstanceOfCastClass = %s\n",
                                                         node->getOpCode().getName(), profiledClassesIt->profiledClass,
                                                         (profiledClassesIt->isProfiledClassInstanceOfCastClass) ? "true" : "false");

               genInstanceOfOrCheckCastArbitraryClassTest(node, objectClassReg, profiledClassesIt->profiledClass, srm, cg);
               /**
                *  At this point EQ flag will be set if the profiledClass matches the cast class.
                *  Set resultReg to 1 if isProfiledClassInstanceOfCastClass is true
                */ 
               if (profiledClassesIt->isProfiledClassInstanceOfCastClass)
                  {
                  generateCSetInstruction(cg, node, resultReg, TR::CC_EQ);
                  }
               profiledClassesIt++;
               if (profiledClassesIt != profiledClassesItEnd)
                  {
                  generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, doneLabel, TR::CC_EQ);
                  }
               }
            }
            break;
         case CompileTimeGuessClassTest:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting CompileTimeGuessClassTest\n", node->getOpCode().getName());
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/compTimeGuess", comp->signature()),1,TR::DebugCounter::Undetermined);
 
            genInstanceOfOrCheckCastArbitraryClassTest(node, objectClassReg, compileTimeGuessClass, srm, cg);
            generateCSetInstruction(cg, node, resultReg, TR::CC_EQ);

            break;
         case CastClassCacheTest:
            {
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting CastClassCacheTest\n", node->getOpCode().getName());
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/CastClassCache", comp->signature()),1,TR::DebugCounter::Undetermined);

            /**
             * Compare the cast class against the cache on the instance class.
             * If they are the same the cast is successful.
             * If not it's either because the cache class does not match the cast class, 
             * or it does match except the cache class has the low bit set, which means the cast is not successful.
             */
            TR::Register *castClassCacheReg = srm->findOrCreateScratchRegister();
            generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, castClassCacheReg,
                              new (cg->trHeapMemory()) TR::MemoryReference(objectClassReg, offsetof(J9Class, castClassCache), cg));
            generateTrg1Src2Instruction(cg, TR::InstOpCode::eorx, node, castClassCacheReg, castClassCacheReg, castClassReg);
            generateCompareImmInstruction(cg, node, castClassCacheReg, 1, true);

            /**
             *  At this point LT flag will be set if the cast is successful, EQ flag will be set if the cast is unsuccessful,
             *  and GT flag will be set if the cache class did not match the cast class.
             */ 
            generateCSetInstruction(cg, node, resultReg, TR::CC_LT);
            srm->reclaimScratchRegister(castClassCacheReg);
            }
            break;
         case ArrayOfJavaLangObjectTest:
            {
            TR_ASSERT_FATAL(isNextItemGoToFalse(it, itEnd), "ArrayOfJavaLangObjectTest is always followed by GoToFalse");
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting ArrayOfJavaLangObjectTest\n", node->getOpCode().getName());
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/ArrayTest", comp->signature()),1,TR::DebugCounter::Undetermined);
            genInstanceOfOrCheckCastObjectArrayTest(node, objectClassReg, doneLabel, true, srm, cg);
            generateCSetInstruction(cg, node, resultReg, TR::CC_EQ);
            }
            break;
         case DynamicCacheObjectClassTest:
            TR_ASSERT_FATAL(false, "%s: DynamicCacheObjectClassTest is not implemented on aarch64\n", node->getOpCode().getName());
            break;
         case DynamicCacheDynamicCastClassTest:
            TR_ASSERT_FATAL(false, "%s: DynamicCacheDynamicCastClassTest is not implemented on aarch64\n", node->getOpCode().getName());
            break;
         case HelperCall:
            {
            TR_ASSERT_FATAL(isTerminalSequence(it, itEnd), "HelperCall should be the terminal sequence");
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting HelperCall\n", node->getOpCode().getName());
            TR_ARM64OutOfLineCodeSection *outlinedHelperCall = new (cg->trHeapMemory()) TR_ARM64OutOfLineCodeSection(node, TR::icall, resultReg, callHelperLabel, doneLabel, cg);

            cg->getARM64OutOfLineCodeSectionList().push_front(outlinedHelperCall);

            if (it == itBegin)
               {
               // If HelperCall is only the item in the sequence, branch to OOL
               generateLabelInstruction(cg, TR::InstOpCode::b, node, callHelperLabel);
               }
            }
            break;
         }

      switch (current)
         {
         case ClassEqualityTest:
         case SuperClassTest:
         case ProfiledClassTest:
         case CompileTimeGuessClassTest:
         case ArrayOfJavaLangObjectTest:
            /**
             * For those tests, EQ flag is set if the cache hit
             */
            if (isNextItemHelperCall(it, itEnd))
               {
               generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, callHelperLabel, TR::CC_NE);
               }
            else if (!isNextItemGoToFalse(it, itEnd))
               {
               // If other tests follow, branch to doneLabel
               generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, doneLabel, TR::CC_EQ);
               }          
            break;
         case CastClassCacheTest:
            if (isNextItemHelperCall(it, itEnd))
               {
               generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, callHelperLabel, TR::CC_GT);
               }
            else if (!isNextItemGoToFalse(it, itEnd))
               {
               generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, doneLabel, TR::CC_LE);
               } 
            break;
         case NullTest:
            break;
         default:
            if (isNextItemHelperCall(it, itEnd))
               {
               generateLabelInstruction(cg, TR::InstOpCode::b, node, callHelperLabel);
               }
            break;
         }

      if (!isTerminalSequence(it, itEnd))
         {
         generateLabelInstruction(cg, TR::InstOpCode::label, node, nextSequenceLabel);
         nextSequenceLabel = generateLabelSymbol(cg);
         }

      }

   if (objectClassReg)
      srm->reclaimScratchRegister(objectClassReg);
   
   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3 + srm->numAvailableRegisters(), cg->trMemory());
   srm->addScratchRegistersToDependencyList(deps);

   deps->addPostCondition(resultReg, TR::RealRegister::NoReg);
   deps->addPostCondition(objectReg, TR::RealRegister::NoReg);

   if (castClassReg)
      {
      deps->addPostCondition(castClassReg, TR::RealRegister::NoReg);
      }
   
   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, deps);

   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfOrCheckCast/%s/fastPath",
                                                               node->getOpCode().getName()),
                            *srm);
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfOrCheckCast.perMethod/%s/(%s)/%d/%d/fastPath",
                                                               node->getOpCode().getName(), comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
                            *srm);


   cg->decReferenceCount(objectNode);
   cg->decReferenceCount(castClassNode);
   // Stop using every reg in the deps except these ones.
   //
   deps->stopUsingDepRegs(cg, objectReg, resultReg);

   node->setRegister(resultReg);

   return resultReg;
   }

/**
 * @brief Generates null test instructions
 * 
 * @param[in]         cg: code generator
 * @param[in]     objReg: register holding object
 * @param[in]       node: null check node
 * @param[in] nullSymRef: symbol reference of null check
 * 
 */
static
void generateNullTest(TR::CodeGenerator *cg, TR::Register *objReg, TR::Node *node, TR::SymbolReference *nullSymRef = NULL)
   {
   TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
   TR::Compilation *comp = cg->comp();
   if (nullSymRef == NULL)
      {
      nullSymRef = comp->getSymRefTab()->findOrCreateNullCheckSymbolRef(comp->getMethodSymbol());
      }
   TR::Snippet *snippet = new (cg->trHeapMemory()) TR::ARM64HelperCallSnippet(cg, node, snippetLabel, nullSymRef, NULL);
   cg->addSnippet(snippet);

   TR::Instruction *cbzInstruction = generateCompareBranchInstruction(cg, TR::InstOpCode::cbzx, node, objReg, snippetLabel);
   cbzInstruction->setNeedsGCMap(0xffffffff);
   snippet->gcMap().setGCRegisterMask(0xffffffff);
   // ARM64HelperCallSnippet generates "bl" instruction
   cg->machine()->setLinkRegisterKilled(true);
   }

TR::Register *
J9::ARM64::TreeEvaluator::VMcheckcastEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation                      *comp = cg->comp();
   TR_OpaqueClassBlock                  *compileTimeGuessClass;
   int32_t                               maxProfiledClasses = comp->getOptions()->getCheckcastMaxProfiledClassTests();
   traceMsg(comp, "%s:Maximum Profiled Classes = %d\n", node->getOpCode().getName(),maxProfiledClasses);
   TR_ASSERT_FATAL(maxProfiledClasses <= 4, "Maximum 4 profiled classes per site allowed because we use a fixed stack allocated buffer for profiled classes\n");
   InstanceOfOrCheckCastSequences        sequences[InstanceOfOrCheckCastMaxSequences];
   bool                                  topClassWasCastClass = false;
   float                                 topClassProbability = 0.0;

   bool                                  profiledClassIsInstanceOf;
   InstanceOfOrCheckCastProfiledClasses  profiledClassesList[4];
   uint32_t                              numberOfProfiledClass;
   uint32_t                              numSequencesRemaining = calculateInstanceOfOrCheckCastSequences(node, sequences, &compileTimeGuessClass, cg, profiledClassesList, &numberOfProfiledClass, maxProfiledClasses, &topClassProbability, &topClassWasCastClass);


   TR::Node                       *objectNode = node->getFirstChild();
   TR::Node                       *castClassNode = node->getSecondChild();
   TR::Register                   *objectReg = cg->evaluate(objectNode);
   TR::Register                   *castClassReg = NULL;

   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *callHelperLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *nextSequenceLabel = generateLabelSymbol(cg);

   TR::Instruction *gcPoint;

   TR_ARM64ScratchRegisterManager *srm = cg->generateScratchRegisterManager();
   TR::Register                 *objectClassReg = NULL;

   auto itBegin = std::begin(sequences);
   const auto itEnd = std::next(itBegin, numSequencesRemaining);
   
   for (auto it = itBegin; it != itEnd; it++)
      {
      auto current = *it;
      switch (current)
         {
         case EvaluateCastClass:
            TR_ASSERT(!castClassReg, "Cast class already evaluated");
            castClassReg = cg->gprClobberEvaluate(castClassNode);
            break;
         case LoadObjectClass:
            TR_ASSERT(!objectClassReg, "Object class already loaded");
            objectClassReg = srm->findOrCreateScratchRegister();
            generateLoadJ9Class(node, objectClassReg, objectReg, cg);
            break;
         case NullTest:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting NullTest\n", node->getOpCode().getName());
            TR_ASSERT(!objectNode->isNonNull(), "Object is known to be non-null, no need for a null test");
            if (node->getOpCodeValue() == TR::checkcastAndNULLCHK)
               {
               TR::Node *nullChkInfo = comp->findNullChkInfo(node);
               generateNullTest(cg, objectReg, nullChkInfo);
               }
            else
               {
               if (isNextItemHelperCall(it, itEnd) || isNextItemGoToFalse(it, itEnd))
                  {
                  generateCompareBranchInstruction(cg, TR::InstOpCode::cbnzx, node, objectReg, callHelperLabel);
                  }
               else
                  {
                  // branch to doneLabel if object is null
                  generateCompareBranchInstruction(cg, TR::InstOpCode::cbzx, node, objectReg, doneLabel);
                  }
               }
            break;
         case GoToTrue:
            TR_ASSERT_FATAL(isTerminalSequence(it, itEnd), "GoToTrue should be the terminal sequence");
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting GoToTrue\n", node->getOpCode().getName());
            break;
         case ClassEqualityTest:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting ClassEqualityTest\n", node->getOpCode().getName());
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/Equality", comp->signature()),1,TR::DebugCounter::Undetermined);

            generateCompareInstruction(cg, node, objectClassReg, castClassReg, true);
            break;
         case SuperClassTest:
            {
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting SuperClassTest\n", node->getOpCode().getName());
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/SuperClassTest", comp->signature()),1,TR::DebugCounter::Undetermined);

            int32_t castClassDepth = castClassNode->getSymbolReference()->classDepth(comp);
            auto falseLabel = (isNextItemGoToFalse(it, itEnd) || isNextItemHelperCall(it, itEnd)) ? callHelperLabel : nextSequenceLabel;
            genSuperClassTest(node, objectClassReg, false, castClassReg, castClassDepth, falseLabel, srm, cg);
            }
            break;
         /**
          *    Following switch case generates sequence of instructions for profiled class test for this checkCast node
          *    arbitraryClassReg1 <= profiledClass
          *    if (arbitraryClassReg1 == objClassReg)
          *       JMP DoneLabel
          *    else
          *       continue to NextTest
          */
         case ProfiledClassTest:
            {
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting ProfiledClassTest\n", node->getOpCode().getName());
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/Profile", comp->signature()),1,TR::DebugCounter::Undetermined);

            auto profiledClassesIt = std::begin(profiledClassesList);
            auto profiledClassesItEnd = std::next(profiledClassesIt, numberOfProfiledClass);
            while (profiledClassesIt != profiledClassesItEnd)
               {
               if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: ProfiledClassTest: profiledClass = %p, isProfiledClassInstanceOfCastClass = %s\n",
                                                         node->getOpCode().getName(), profiledClassesIt->profiledClass,
                                                         (profiledClassesIt->isProfiledClassInstanceOfCastClass) ? "true" : "false");

               genInstanceOfOrCheckCastArbitraryClassTest(node, objectClassReg, profiledClassesIt->profiledClass, srm, cg);
               /**
                *  At this point EQ flag will be set if the profiledClass matches the cast class.
                */ 
               profiledClassesIt++;
               if (profiledClassesIt != profiledClassesItEnd)
                  {
                  generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, doneLabel, TR::CC_EQ);
                  }
               }
            }
            break;
         case CompileTimeGuessClassTest:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting CompileTimeGuessClassTest\n", node->getOpCode().getName());
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/compTimeGuess", comp->signature()),1,TR::DebugCounter::Undetermined);
 
            genInstanceOfOrCheckCastArbitraryClassTest(node, objectClassReg, compileTimeGuessClass, srm, cg);
            break;
         /**
          *    Following switch case generates sequence of instructions for cast class cache test for this checkCast node
          *    Load castClassCacheReg, offsetOf(J9Class,castClassCache)
          *    if castClassCacheReg == castClassReg
          *       JMP DoneLabel
          *    else
          *       continue to NextTest
          */
         case CastClassCacheTest:
            {
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting CastClassCacheTest\n", node->getOpCode().getName());
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/CastClassCache", comp->signature()),1,TR::DebugCounter::Undetermined);

            /**
             * Compare the cast class against the cache on the instance class.
             * If they are the same the cast is successful.
             * If not it's either because the cache class does not match the cast class, 
             * or it does match except the cache class has the low bit set, which means the cast is not successful.
             * In those cases, we need to call out to helper.
             */
            TR::Register *castClassCacheReg = srm->findOrCreateScratchRegister();
            generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, castClassCacheReg,
                              new (cg->trHeapMemory()) TR::MemoryReference(objectClassReg, offsetof(J9Class, castClassCache), cg));
            generateCompareInstruction(cg, node, castClassCacheReg, castClassReg, true);
            /**
             *  At this point, EQ flag will be set if the cast is successful.
             */ 
            srm->reclaimScratchRegister(castClassCacheReg);
            }
            break;
         case ArrayOfJavaLangObjectTest:
            {
            TR_ASSERT_FATAL(isNextItemGoToFalse(it, itEnd), "ArrayOfJavaLangObjectTest is always followed by GoToFalse");
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting ArrayOfJavaLangObjectTest\n", node->getOpCode().getName());
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/ArrayTest", comp->signature()),1,TR::DebugCounter::Undetermined);

            /*
             * In this case, the false label is in the OOLCodeSection, and it can be placed far away from here.
             * The offset of tbz/tbnz instruction must be within +-32KB range, so we do not use tbz/tbnz.
             */
            genInstanceOfOrCheckCastObjectArrayTest(node, objectClassReg, callHelperLabel, false, srm, cg);
            }
            break;
         case DynamicCacheObjectClassTest:
            TR_ASSERT_FATAL(false, "%s: DynamicCacheObjectClassTest is not implemented on aarch64\n", node->getOpCode().getName());
            break;
         case DynamicCacheDynamicCastClassTest:
            TR_ASSERT_FATAL(false, "%s: DynamicCacheDynamicCastClassTest is not implemented on aarch64\n", node->getOpCode().getName());
            break;
         case GoToFalse:
         case HelperCall:
            {
            auto seq = (current == GoToFalse) ? "GoToFalse" : "HelperCall";
            TR_ASSERT_FATAL(isTerminalSequence(it, itEnd), "%s should be the terminal sequence", seq);
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting %s\n", node->getOpCode().getName(), seq);
            TR_ARM64OutOfLineCodeSection *outlinedHelperCall = new (cg->trHeapMemory()) TR_ARM64OutOfLineCodeSection(node, TR::call, NULL, callHelperLabel, doneLabel, cg);

            cg->getARM64OutOfLineCodeSectionList().push_front(outlinedHelperCall);

            if (it == itBegin)
               {
               // If HelperCall or GoToFalse is the only item in the sequence, branch to OOL
               generateLabelInstruction(cg, TR::InstOpCode::b, node, callHelperLabel);
               }
            }
            break;
         }

      switch (current)
         {
         case ClassEqualityTest:
         case SuperClassTest:
         case ProfiledClassTest:
         case CompileTimeGuessClassTest:
         case CastClassCacheTest:
         case ArrayOfJavaLangObjectTest:
            /**
             * For those tests, EQ flag is set if the cast is successful
             */
            if (isNextItemHelperCall(it, itEnd) || isNextItemGoToFalse(it, itEnd))
               {
               generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, callHelperLabel, TR::CC_NE);
               }
            else
               {
               // When other tests follow, branch to doneLabel if EQ flag is set
               generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, doneLabel, TR::CC_EQ);
               }
            break;
         case NullTest:
            break;
         default:
            if (isNextItemHelperCall(it, itEnd) || isNextItemGoToFalse(it, itEnd))
               {
               generateLabelInstruction(cg, TR::InstOpCode::b, node, callHelperLabel);
               }
         }

      if (!isTerminalSequence(it, itEnd))
         {
         generateLabelInstruction(cg, TR::InstOpCode::label, node, nextSequenceLabel);
         nextSequenceLabel = generateLabelSymbol(cg);
         }

      }

   if (objectClassReg)
      srm->reclaimScratchRegister(objectClassReg);
   
   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3 + srm->numAvailableRegisters(), cg->trMemory());
   srm->addScratchRegistersToDependencyList(deps);

   deps->addPostCondition(objectReg, TR::RealRegister::NoReg);

   if (castClassReg)
      {
      deps->addPostCondition(castClassReg, TR::RealRegister::NoReg);
      }
   
   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, deps);

   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfOrCheckCast/%s/fastPath",
                                                               node->getOpCode().getName()),
                            *srm);
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfOrCheckCast.perMethod/%s/(%s)/%d/%d/fastPath",
                                                               node->getOpCode().getName(), comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
                            *srm);


   cg->decReferenceCount(objectNode);
   cg->decReferenceCount(castClassNode);
   // Stop using every reg in the deps except objectReg
   //
   deps->stopUsingDepRegs(cg, objectReg);

   node->setRegister(NULL);

   return NULL;
   }

TR::Register *
J9::ARM64::TreeEvaluator::checkcastAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return VMcheckcastEvaluator(node, cg);
   }

TR::Register *
J9::ARM64::TreeEvaluator::checkcastEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return VMcheckcastEvaluator(node, cg);
   }

TR::Register *
J9::ARM64::TreeEvaluator::flushEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes op = node->getOpCodeValue();

   if (op == TR::allocationFence)
      {
      if (!node->canOmitSync())
         {
         // StoreStore barrier is required after publishing new object reference to other threads.
         // dmb ishst (Inner Shareable store barrier)
         generateSynchronizationInstruction(cg, TR::InstOpCode::dmb, node, 0xA);
         }
      }
   else
      {
      uint32_t imm;
      if (op == TR::loadFence)
         {
         // TR::loadFence is used for both loadLoadFence and acquireFence.
         // Loads before the barrier are ordered before loads/stores after the barrier.
         // dmb ishld (Inner Shareable load barrier)
         imm = 0x9;
         }
      else if (op == TR::storeFence)
         {
         // TR::storeFence is used for both storeStoreFence and releaseFence.
         // Loads/Stores before the barrier are ordered before stores after the barrier.
         // dmb ish (Inner Shareable full barrier)
         imm = 0xB;
         }
      else
         {
         // TR::fullFence is used for fullFence.
         // dmb ish (Inner Shareable full barrier)
         imm = 0xB;
         }
      generateSynchronizationInstruction(cg, TR::InstOpCode::dmb, node, imm);
      }

   return NULL;
   }

/**
 * Helper template function to get value clamped between low and high.
 * std::clamp is unavailable for C++11.
 */
template<typename T>
const T clamp(const T& value, const T& low, const T& high)
   {
   return std::min(std::max(value, low), high);
   }

template<typename T>
const T clamp(const int& value, const T& low, const T& high)
   {
   return static_cast<T>(std::min(std::max(value, static_cast<int>(low)), static_cast<int>(high)));
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
   static const char *pTLHPrefetchThresholdSize = feGetEnv("TR_AArch64PrefetchThresholdSize");
   static const char *pTLHPrefetchArrayLineCount = feGetEnv("TR_AArch64PrefetchArrayLineCount");
   static const char *pTLHPrefetchType = feGetEnv("TR_AArch64PrefetchType");
   static const char *pTLHPrefetchTarget = feGetEnv("TR_AArch64PrefetchTarget");
   static const char *pTLHPrefetchPolicy = feGetEnv("TR_AArch64PrefetchPolicy");
   static const int cacheLineSize = (TR::Options::_TLHPrefetchLineSize > 0) ? TR::Options::_TLHPrefetchLineSize : 64;
   static const int tlhPrefetchLineCount = (TR::Options::_TLHPrefetchLineCount > 0) ? TR::Options::_TLHPrefetchLineCount : 1;
   static const int tlhPrefetchStaggeredLineCount = (TR::Options::_TLHPrefetchStaggeredLineCount > 0) ? TR::Options::_TLHPrefetchStaggeredLineCount : 4;
   static const int tlhPrefetchThresholdSize = (pTLHPrefetchThresholdSize) ? atoi(pTLHPrefetchThresholdSize) : 64;
   static const int tlhPrefetchArrayLineCount = (pTLHPrefetchArrayLineCount) ? atoi(pTLHPrefetchArrayLineCount) : 4;
   static const ARM64PrefetchType tlhPrefetchType = (pTLHPrefetchType) ? clamp(atoi(pTLHPrefetchType), ARM64PrefetchType::LOAD, ARM64PrefetchType::STORE)
                                                                  : ARM64PrefetchType::STORE;
   static const ARM64PrefetchTarget tlhPrefetchTarget = (pTLHPrefetchTarget) ? clamp(atoi(pTLHPrefetchTarget), ARM64PrefetchTarget::L1, ARM64PrefetchTarget::L3)
                                                                  : ARM64PrefetchTarget::L3;
   static const ARM64PrefetchPolicy tlhPrefetchPolicy = (pTLHPrefetchPolicy) ? clamp(atoi(pTLHPrefetchPolicy), ARM64PrefetchPolicy::KEEP, ARM64PrefetchPolicy::STRM)
                                                                  : ARM64PrefetchPolicy::STRM;

   TR::Compilation *comp = cg->comp();
   TR::Register *metaReg = cg->getMethodMetaDataRegister();

   uint32_t maxSafeSize = cg->getMaxObjectSizeGuaranteedNotToOverflow();
   bool isTooSmallToPrefetch = false;

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
       * addimmx  tempReg, tempReg, #headerSize+round-1
       * cmpimmw  lengthReg, 0; # of array elements
       * andimmx  tempReg, tempReg, #-round
       * movzx    tempReg2, aligned(#sizeOfDiscontiguousArrayHeader)
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
      static const int32_t objectAlignmentInBytes = TR::Compiler->om.objectAlignmentInBytes();
      bool headerAligned = (allocSize % objectAlignmentInBytes) == 0;
      // zero indicates no rounding is necessary
      const int32_t round = ((elementSize >= objectAlignmentInBytes) && headerAligned) ? 0 : objectAlignmentInBytes;

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
         static const int32_t zeroArraySizeAligned = OMR::align(TR::Compiler->om.discontiguousArrayHeaderSizeInBytes(), objectAlignmentInBytes);
         loadConstant64(cg, node, zeroArraySizeAligned, heapTopReg);

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
      isTooSmallToPrefetch = allocSize < tlhPrefetchThresholdSize;
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

   if (comp->getOption(TR_TLHPrefetch) && (!isTooSmallToPrefetch))
      {
      int offset = tlhPrefetchStaggeredLineCount * cacheLineSize;
      int loopCount = (node->getOpCodeValue() == TR::New) ? tlhPrefetchLineCount : tlhPrefetchArrayLineCount;

      for (int i = 0; i < loopCount; i++)
         {
         generateMemImmInstruction(cg, TR::InstOpCode::prfmimm, node,
            new (cg->trHeapMemory()) TR::MemoryReference(tempReg, offset, cg), toPrefetchOp(tlhPrefetchType, tlhPrefetchTarget, tlhPrefetchPolicy));
         offset += cacheLineSize;
         }
      }
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
 * @param[in] zeroReg:    the register whose value is zero
 * @param[in] tempReg1:   temporary register 1
 * @param[in] isTLHHasNotBeenCleared: true if TLH has not been cleared
 */
static void
genInitObjectHeader(TR::Node *node, TR::CodeGenerator *cg, TR_OpaqueClassBlock *clazz, TR::Register *objectReg, TR::Register *classReg, TR::Register *zeroReg, TR::Register *tempReg1, bool isTLHHasNotBeenCleared)
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

   int32_t lwOffset = fej9->getByteOffsetToLockword(clazz);
   if (clazz && (lwOffset > 0))
      {
      int32_t lwInitialValue = fej9->getInitialLockword(clazz);

      if ((0 != lwInitialValue) || isTLHHasNotBeenCleared)
         {
         bool isCompressedLockWord = fej9->generateCompressedLockWord();
         if (0 != lwInitialValue)
            {
            loadConstant64(cg, node, lwInitialValue, tempReg1);
            generateMemSrc1Instruction(cg, isCompressedLockWord ? TR::InstOpCode::strimmw : TR::InstOpCode::strimmx,
                  node, new (cg->trHeapMemory()) TR::MemoryReference(objectReg, lwOffset, cg), tempReg1);
            }
         else
            {
            generateMemSrc1Instruction(cg, isCompressedLockWord ? TR::InstOpCode::strimmw : TR::InstOpCode::strimmx,
                  node, new (cg->trHeapMemory()) TR::MemoryReference(objectReg, lwOffset, cg), zeroReg);
            }
         }
      }
   }

/**
 * @brief Generates instructions for initializing array header for newarray/anewarray
 *
 * @param[in] node:                   node
 * @param[in] cg:                     code generator
 * @param[in] clazz:                  class pointer to store in the object header
 * @param[in] objectReg:              the register that holds object address
 * @param[in] classReg:               the register that holds class
 * @param[in] sizeReg:                the register that holds array length.
 * @param[in] zeroReg:                the register whose value is zero
 * @param[in] tempReg1:               temporary register 1
 * @param[in] isBatchClearTLHEnabled: true if BatchClearTLH is enabled 
 * @param[in] isTLHHasNotBeenCleared: true if TLH has not been cleared
 */
static void
genInitArrayHeader(TR::Node *node, TR::CodeGenerator *cg, TR_OpaqueClassBlock *clazz, TR::Register *objectReg, TR::Register *classReg, TR::Register *sizeReg, TR::Register *zeroReg, TR::Register *tempReg1,
                  bool isBatchClearTLHEnabled, bool isTLHHasNotBeenCleared)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());

   genInitObjectHeader(node, cg, clazz, objectReg, classReg, zeroReg, tempReg1, isTLHHasNotBeenCleared);
   if (node->getFirstChild()->getOpCode().isLoadConst() && (node->getFirstChild()->getInt() == 0))
      {
      // If BatchClearTLH is enabled, we do not need to write 0 into the header.
      if (!isBatchClearTLHEnabled)
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
      }
   else
      {
      // Store the array size
      // If the size field of contiguous array header is 0, the array is discontiguous and
      // the size of discontiguous array must be in the size field of discontiguous array header.
      // For now, we do not create non-zero length discontigous array,
      // so it is safe to write 0 into the size field of discontiguous array header.
      //
      // In the compressedrefs build, the size field of discontigous array header is cleared by instructions generated by genZeroInit().
      // In the large heap build, we must clear size and mustBeZero field here
      if (TR::Compiler->om.generateCompressedObjectHeaders())
         {
         generateMemSrc1Instruction(cg, TR::InstOpCode::strimmw, node,
                                    new (cg->trHeapMemory()) TR::MemoryReference(objectReg, fej9->getOffsetOfContiguousArraySizeField(), cg),
                                    sizeReg);
         if (!isTLHHasNotBeenCleared)
            {
            // If BatchClearTLH is not enabled and TLH has not been cleared, write 0 into the size field of J9IndexableObjectDiscontiguousCompressed.
            generateMemSrc1Instruction(cg, TR::InstOpCode::strimmw, node,
                           new (cg->trHeapMemory()) TR::MemoryReference(objectReg, fej9->getOffsetOfDiscontiguousArraySizeField(), cg),
                           zeroReg);
            }
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

   bool generateArraylets = comp->generateArraylets();

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
      if (generateArraylets || TR::Compiler->om.useHybridArraylets())
         {
         if (node->getOpCodeValue() == TR::newarray)
            elementSize = TR::Compiler->om.getSizeOfArrayElement(node);
         else if (comp->useCompressedPointers())
            elementSize = TR::Compiler->om.sizeofReferenceField();
         else
            elementSize = TR::Compiler->om.sizeofReferenceAddress();

         if (generateArraylets)
            headerSize = fej9->getArrayletFirstElementOffset(elementSize, comp);
         else
            headerSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
         }
      else
         {
         elementSize = TR::Compiler->om.getSizeOfArrayElement(node);
         headerSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
         }

      // If the array cannot be allocated as a contiguous array, then comp->canAllocateInline should have returned -1.
      // The only exception is when the array length is 0.
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
   const bool isBatchClearTLHEnabled = fej9->tlhHasBeenCleared();
   if (!isBatchClearTLHEnabled)
      {
      // TODO selectively initialize necessary slots
      if (!node->canSkipZeroInitialization())
         {
         genZeroInitObject(node, cg, isVariableLength, objectSize, headerSize, resultReg, tempReg3, zeroReg, tempReg1, tempReg2);
         }
      }
   const bool tlhHasNotBeenCleared = (!isBatchClearTLHEnabled) && node->canSkipZeroInitialization();

   // 8. Initialize Object Header
   if (isArrayNew)
      {
      genInitArrayHeader(node, cg, clazz, resultReg, classReg, lengthReg, zeroReg, tempReg1, isBatchClearTLHEnabled, tlhHasNotBeenCleared);

      if (generateArraylets)
         {
         // write arraylet pointer to object header
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addimmx, node, tempReg2, resultReg, headerSize);
         if (TR::Compiler->om.compressedReferenceShiftOffset() > 0)
            generateLogicalShiftRightImmInstruction(cg, node, tempReg2, tempReg2, TR::Compiler->om.compressedReferenceShiftOffset());

         TR::InstOpCode::Mnemonic storeOp = comp->useCompressedPointers() ? TR::InstOpCode::strimmx : TR::InstOpCode::strimmw;
         generateMemSrc1Instruction(cg, storeOp, node,
                                    new (cg->trHeapMemory()) TR::MemoryReference(resultReg, fej9->getFirstArrayletPointerOffset(comp), cg),
                                    tempReg2);
         }
      }
   else
      {
      genInitObjectHeader(node, cg, clazz, resultReg, classReg, zeroReg, tempReg1, tlhHasNotBeenCleared);
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
   TR_YesNoMaybe isMonitorValueBasedOrValueType = cg->isMonitorValueBasedOrValueType(node);

   if (comp->getOption(TR_FullSpeedDebug) ||
       (isMonitorValueBasedOrValueType == TR_yes) ||
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

   // If object is not known to be value type or value based class at compile time, check at run time
   if (isMonitorValueBasedOrValueType == TR_maybe)
      {
      generateCheckForValueMonitorEnterOrExit(node, callLabel, objReg, tempReg, dataReg, cg, J9_CLASS_DISALLOWS_LOCKING_FLAGS);
      }

   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addimmx, node, addrReg, objReg, lwOffset); // ldxr/stxr instructions does not take immediate offset

   TR::Instruction *faultingInstruction;
   static const bool disableLSE = feGetEnv("TR_aarch64DisableLSE") != NULL;
   if (comp->target().cpu.supportsFeature(OMR_FEATURE_ARM64_LSE) && (!disableLSE))
      {
      generateTrg1ImmInstruction(cg, TR::InstOpCode::movzx, node, dataReg, 0); // expected value
      /*
       * We need to generate a CASAL, not a CASA because loads/stores before monitor exit can be reordered after a CASA
       * as the store to lockword for monitor exit is a plain store.
       */
      op = fej9->generateCompressedLockWord() ? TR::InstOpCode::casalw : TR::InstOpCode::casalx;
      /*
       * As Trg1MemSrc1Instruction was introduced to support ldxr/stxr instructions, target and source register convention
       * is somewhat confusing. Its `treg` register actually is a source register and `sreg` register is a target register.
       * This needs to be fixed at some point.
       */
      faultingInstruction = generateTrg1MemSrc1Instruction(cg, op, node, dataReg, new (cg->trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg), metaReg);
      generateCompareBranchInstruction(cg, TR::InstOpCode::cbnzx, node, dataReg, incLabel);
      }
   else
      {
      generateLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel);
      op = fej9->generateCompressedLockWord() ? TR::InstOpCode::ldxrw : TR::InstOpCode::ldxrx;
      faultingInstruction = generateTrg1MemInstruction(cg, op, node, dataReg, new (cg->trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg));

      generateCompareBranchInstruction(cg, TR::InstOpCode::cbnzx, node, dataReg, incLabel);
      op = fej9->generateCompressedLockWord() ? TR::InstOpCode::stxrw : TR::InstOpCode::stxrx;

      generateTrg1MemSrc1Instruction(cg, op, node, tempReg, new (cg->trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg), metaReg);
      generateCompareBranchInstruction(cg, TR::InstOpCode::cbnzx, node, tempReg, loopLabel);

      generateSynchronizationInstruction(cg, TR::InstOpCode::dmb, node, 0xB); // dmb ish (Inner Shareable full barrier)
      }

   // InstructonDelegate::setupImplicitNullPointerException checks if the memory reference uses nullcheck reference register.
   // In this case, nullcheck reference register is objReg, but the memory reference does not use it,
   // thus we need to explicitly set implicit exception point here.
   if (cg->getHasResumableTrapHandler() && cg->getCurrentEvaluationTreeTop()->getNode()->getOpCode().isNullCheck())
      {
      if (cg->getImplicitExceptionPoint() == NULL)
         {
         if (comp->getOption(TR_TraceCG))
            {
            traceMsg(comp, "Instruction %p throws an implicit NPE, node: %p NPE node: %p\n", faultingInstruction, node, objNode);
            }
         cg->setImplicitExceptionPoint(faultingInstruction);
         }
      }

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
   TR_ASSERT(cg->comp()->requiresSpineChecks(), "TR::arraylength should be lowered when hybrid arraylets are not in use");

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

      // If this BNDCHK is combined with previous NULLCHK, there is
      // an instruction that will cause a hardware trap if the exception is to be
      // taken. If this method may catch the exception, a GC stack map must be
      // created for this instruction. All registers are valid at this GC point
      // TODO - if the method may not catch the exception we still need to note
      // that the GC point exists, since maps before this point and after it cannot
      // be merged.
      //
      if (cg->getHasResumableTrapHandler() && node->hasFoldedImplicitNULLCHK())
         {
         TR::Compilation *comp = cg->comp();
         TR::Instruction *faultingInstruction = cg->getImplicitExceptionPoint();
         if (comp->getOption(TR_TraceCG))
            {
            traceMsg(comp, "\nNode %p has foldedimplicitNULLCHK, and a faulting instruction of %p\n", node, faultingInstruction);
            }

         if (faultingInstruction)
            {
            faultingInstruction->setNeedsGCMap(0xffffffff);
            cg->machine()->setLinkRegisterKilled(true);

            TR_Debug * debugObj = cg->getDebug();
            if (debugObj)
               {
               debugObj->addInstructionComment(faultingInstruction, "Throws Implicit Null Pointer Exception");
               }
            }
         }
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

/**
 * @brief Generate instruction sequence for array store check
 * 
 * @param[in] node:            node
 * @param[in] srcReg:          register contains source object
 * @param[in] dstReg:          register contains destination array
 * @param[in] srm:             scratch register manager
 * @param[in] doneLabel:       label to jump when check is successful
 * @param[in] helperCallLabel: label to jump when helper call is needed
 * @param[in] cg:              code generator
 */
static void VMarrayStoreCHKEvaluator(TR::Node *node, TR::Register *srcReg, TR::Register *dstReg, TR_ARM64ScratchRegisterManager *srm,
                                     TR::LabelSymbol *doneLabel, TR::LabelSymbol *helperCallLabel, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VM *fej9 = reinterpret_cast<TR_J9VM *>(cg->fe());
   TR::Register *sourceClassReg = srm->findOrCreateScratchRegister();
   TR::Register *destArrayClassReg = srm->findOrCreateScratchRegister();

   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "ArrayStoreCHKEvaluator:010VMarrayStoreCHKEvaluator"), *srm);

   generateLoadJ9Class(node, sourceClassReg, srcReg, cg);
   generateLoadJ9Class(node, destArrayClassReg, dstReg, cg);

   TR::Register *destComponentClassReg = srm->findOrCreateScratchRegister();
   TR_Debug *debugObj = cg->getDebug();

   auto instr = generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, destComponentClassReg,
         new (cg->trHeapMemory()) TR::MemoryReference(destArrayClassReg, static_cast<int32_t>(offsetof(J9ArrayClass, componentType)), cg));
   if (debugObj)
      {
      debugObj->addInstructionComment(instr, "load component type of the destination array");
      }
   srm->reclaimScratchRegister(destArrayClassReg);
   destArrayClassReg = NULL; // prevent re-using this register by error

   generateCompareInstruction(cg, node, destComponentClassReg, sourceClassReg, true);
   instr = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, doneLabel, TR::CC_EQ);
   if (debugObj)
      {
      debugObj->addInstructionComment(instr, "done if component type of the destination array equals to source object class");
      }
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "ArrayStoreCHKEvaluator:010VMarrayStoreCHKEvaluator:01ClassEqualityCheckDone"), *srm);

   TR_OpaqueClassBlock *objectClass = fej9->getSystemClassFromClassName("java/lang/Object", 16, true);
   /*
    * objectClass is used for Object arrays check optimization: when we are storing to Object arrays we can skip all other array store checks
    * However, TR_J9SharedCacheVM::getSystemClassFromClassName can return 0 when it's impossible to relocate j9class later for AOT loads
    * in that case we don't want to generate the Object arrays check
    */
   bool doObjectArrayCheck = objectClass != NULL;
   if (doObjectArrayCheck)
      {
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "ArrayStoreCHKEvaluator:010VMarrayStoreCHKEvaluator:02JavaLangObjectCheck"), *srm);

      TR::Register *javaLangObjectClassReg = srm->findOrCreateScratchRegister();
      if (cg->wantToPatchClassPointer(objectClass, node) || cg->needClassAndMethodPointerRelocations())
         {
         loadAddressConstantInSnippet(cg, node, reinterpret_cast<intptr_t>(objectClass), javaLangObjectClassReg, TR_ClassPointer);
         }
      else
         {
         loadAddressConstant(cg, node, reinterpret_cast<intptr_t>(objectClass), javaLangObjectClassReg);
         }
      generateCompareInstruction(cg, node, javaLangObjectClassReg, destComponentClassReg, true);
      instr = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, doneLabel, TR::CC_EQ);
      if (debugObj)
         {
         debugObj->addInstructionComment(instr, "done if component type of the destination array equals to java/lang/Object");
         }
      srm->reclaimScratchRegister(javaLangObjectClassReg);

      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "ArrayStoreCHKEvaluator:010VMarrayStoreCHKEvaluator:03JavaLangObjectCheckDone"), *srm);
      }

   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "ArrayStoreCHKEvaluator:010VMarrayStoreCHKEvaluator:04CastClassCacheCheck"), *srm);

   TR::Register *castClassCacheReg = srm->findOrCreateScratchRegister();
   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, castClassCacheReg,
                              new (cg->trHeapMemory()) TR::MemoryReference(sourceClassReg, offsetof(J9Class, castClassCache), cg));
   generateCompareInstruction(cg, node, castClassCacheReg, destComponentClassReg, true);
   instr = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, doneLabel, TR::CC_EQ);
   if (debugObj)
      {
      debugObj->addInstructionComment(instr, "done if component type of the destination array equals to castClassCache of source object class");
      }
   srm->reclaimScratchRegister(castClassCacheReg);
   castClassCacheReg = NULL; // prevent re-using this register by error

   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "ArrayStoreCHKEvaluator:010VMarrayStoreCHKEvaluator:05CastClassCacheCheckDone"), *srm);

   /*
    * If isInstanceOf (objectClass,ArrayComponentClass,true,true) was successful and stored during VP, we need to test again the real arrayComponentClass
    * Need to relocate address of arrayComponentClass under AOT compilation.
    * Need to add PICsite on class constant if the class can be unloaded.
    */
   if (node->getArrayComponentClassInNode())
      {
      TR::Register *arrayComponentClassReg = srm->findOrCreateScratchRegister();
      TR_OpaqueClassBlock *arrayComponentClass = node->getArrayComponentClassInNode();
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "ArrayStoreCHKEvaluator:010VMarrayStoreCHKEvaluator:06ArrayComponentClassCheck"), *srm);

      if (cg->wantToPatchClassPointer(arrayComponentClass, node) || cg->needClassAndMethodPointerRelocations())
         {
         loadAddressConstantInSnippet(cg, node, reinterpret_cast<intptr_t>(arrayComponentClass), arrayComponentClassReg, TR_ClassPointer);
         }
      else
         {
         bool isUnloadAssumptionRequired = fej9->isUnloadAssumptionRequired(arrayComponentClass, comp->getCurrentMethod());

         if (isUnloadAssumptionRequired)
            {
            loadAddressConstantInSnippet(cg, node, reinterpret_cast<intptr_t>(arrayComponentClass), arrayComponentClassReg, TR_NoRelocation, true);
            }
         else
            {
            loadAddressConstant(cg, node, reinterpret_cast<intptr_t>(arrayComponentClass), arrayComponentClassReg, NULL, true);
            }
         }
      generateCompareInstruction(cg, node, arrayComponentClassReg, destComponentClassReg, true);
      instr = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, doneLabel, TR::CC_EQ);

      if (debugObj)
         {
         debugObj->addInstructionComment(instr, "done if component type of the destination array equals to arrayComponentClass set in node");
         }
      srm->reclaimScratchRegister(arrayComponentClassReg);

      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "ArrayStoreCHKEvaluator:010VMarrayStoreCHKEvaluator:06ArrayComponentClassCheckDone"), *srm);
      }

   genSuperClassTest(node, sourceClassReg, true, destComponentClassReg, -1, helperCallLabel, srm, cg);
   srm->reclaimScratchRegister(destComponentClassReg);

   // prevent re-using these registers by error
   sourceClassReg = NULL;
   destComponentClassReg = NULL;

   instr = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, helperCallLabel, TR::CC_NE);
   if (debugObj)
      {
      debugObj->addInstructionComment(instr, "Call helper if super class test fails");
      }
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "ArrayStoreCHKEvaluator:010VMarrayStoreCHKEvaluator:07SuperClassTestDone"), *srm);

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
      usingCompressedPointers = true;

      while ((sourceChild->getNumChildren() > 0) && (sourceChild->getOpCodeValue() != TR::a2l))
         sourceChild = sourceChild->getFirstChild();
      if (sourceChild->getOpCodeValue() == TR::a2l)
         sourceChild = sourceChild->getFirstChild();
      }

   TR::Register *srcReg = cg->evaluate(sourceChild);
   TR::Register *dstReg = cg->evaluate(dstNode);
   TR_ARM64ScratchRegisterManager *srm = cg->generateScratchRegisterManager();

   TR::LabelSymbol *wbLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *OOLMergeLabel = generateLabelSymbol(cg);
   TR_Debug * debugObj = cg->getDebug();

   if (!sourceChild->isNull())
      {
      static const bool disableArrayStoreCHKOpts = comp->getOption(TR_DisableArrayStoreCheckOpts);
      TR_J9VM *fej9 = reinterpret_cast<TR_J9VM *>(cg->fe());
      TR::LabelSymbol *helperCallLabel = generateLabelSymbol(cg);
      // Since ArrayStoreCHK doesn't have the shape of the corresponding helper call we have to create this tree
      // so we can have it evaluated out of line
      TR::Node *helperCallNode = TR::Node::createWithSymRef(node, TR::call, 2, node->getSymbolReference());
      helperCallNode->setAndIncChild(0, sourceChild);
      helperCallNode->setAndIncChild(1, dstNode);
      if (comp->getOption(TR_TraceCG))
         {
         traceMsg(comp, "%s: Creating and evaluating the following tree to generate the necessary helper call for this node\n", node->getOpCode().getName());
         cg->getDebug()->print(comp->getOutFile(), helperCallNode);
         }

      bool nopASC = node->getArrayStoreClassInNode() && comp->performVirtualGuardNOPing() &&
                     (!fej9->classHasBeenExtended(node->getArrayStoreClassInNode())) && (!disableArrayStoreCHKOpts);
      if (nopASC)
         {
         // Speculatively NOP the array store check if VP is able to prove that the ASC
         // would always succeed given the current state of the class hierarchy.
         //
         TR_VirtualGuard *virtualGuard = TR_VirtualGuard::createArrayStoreCheckGuard(comp, node, node->getArrayStoreClassInNode());
         TR::Instruction *vgnopInstr = generateVirtualGuardNOPInstruction(cg, node, virtualGuard->addNOPSite(), NULL, helperCallLabel);
         }
      else
         {
         // If source is null, we can skip array store check.
         auto cbzInstruction = generateCompareBranchInstruction(cg, TR::InstOpCode::cbzx, node, srcReg, wbLabel);
         if (debugObj)
            {
            debugObj->addInstructionComment(cbzInstruction, "jump past array store check");
            }
         if (!disableArrayStoreCHKOpts)
            {
            VMarrayStoreCHKEvaluator(node, srcReg, dstReg, srm, wbLabel, helperCallLabel, cg);
            }
         else
            {
            generateLabelInstruction(cg, TR::InstOpCode::b, node, helperCallLabel);
            }
         }

      TR_ARM64OutOfLineCodeSection *outlinedHelperCall = new (cg->trHeapMemory()) TR_ARM64OutOfLineCodeSection(helperCallNode, TR::call, NULL, helperCallLabel, OOLMergeLabel, cg);
      cg->getARM64OutOfLineCodeSectionList().push_front(outlinedHelperCall);
      cg->decReferenceCount(helperCallNode->getFirstChild());
      cg->decReferenceCount(helperCallNode->getSecondChild());
      }
   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2 + srm->numAvailableRegisters(), cg->trMemory());
   srm->addScratchRegistersToDependencyList(deps);

   deps->addPostCondition(srcReg, TR::RealRegister::NoReg);
   deps->addPostCondition(dstReg, TR::RealRegister::NoReg);
   auto instr = generateLabelInstruction(cg, TR::InstOpCode::label, node, wbLabel);
   if (debugObj)
      {
      debugObj->addInstructionComment(instr, "ArrayStoreCHK Done");
      }
   instr = generateLabelInstruction(cg, TR::InstOpCode::label, node, OOLMergeLabel, deps);
   if (debugObj)
      {
      debugObj->addInstructionComment(instr, "OOL merge point");
      }

   srm->stopUsingRegisters();

   cg->evaluate(firstChild);

   cg->decReferenceCount(firstChild);

   return NULL;
   }

static TR::Register *
genCAS(TR::Node *node, TR::CodeGenerator *cg, TR_ARM64ScratchRegisterManager *srm, TR::Register *objReg, TR::Register *offsetReg, intptr_t offset, bool offsetInReg, TR::Register *oldVReg, TR::Register *newVReg,
      TR::LabelSymbol *doneLabel, int32_t oldValue, bool oldValueInReg, bool is64bit, bool casWithoutSync = false)
   {
   TR::Compilation * comp = cg->comp();
   TR::Register *addrReg = srm->findOrCreateScratchRegister();
   TR::Register *resultReg = cg->allocateRegister();
   TR::InstOpCode::Mnemonic op;


   if (offsetInReg)
      {
      generateTrg1Src2Instruction(cg, TR::InstOpCode::addx, node, addrReg, objReg, offsetReg); // ldxr/stxr instructions does not take offset
      }
   else
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addimmx, node, addrReg, objReg, offset); // ldxr/stxr instructions does not take offset
      }

   const bool createDoneLabel = (doneLabel == NULL);

   static const bool disableLSE = feGetEnv("TR_aarch64DisableLSE") != NULL;
   if (comp->target().cpu.supportsFeature(OMR_FEATURE_ARM64_LSE) && (!disableLSE))
      {
      TR_ASSERT_FATAL(oldValueInReg, "Expecting oldValue is in register if LSE is enabled");
      /*
       * movx   resultReg, oldVReg
       * casal  resultReg, newVReg, [addrReg]
       * cmp    resultReg, oldReg
       * cset   resultReg, eq
       */
      generateMovInstruction(cg, node, resultReg, oldVReg, is64bit);
      op = casWithoutSync ? (is64bit ? TR::InstOpCode::casx : TR::InstOpCode::casw) : (is64bit ? TR::InstOpCode::casalx : TR::InstOpCode::casalw);
      generateTrg1MemSrc1Instruction(cg, op, node, resultReg, new (cg->trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg), newVReg);
      generateCompareInstruction(cg, node, resultReg, oldVReg, is64bit);
      generateCSetInstruction(cg, node, resultReg, TR::CC_EQ);
      if (!createDoneLabel)
         {
         generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, doneLabel, TR::CC_NE);
         }
      }
   else
      {
      TR::LabelSymbol *loopLabel = generateLabelSymbol(cg);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel);
      if (createDoneLabel)
         {
         doneLabel = generateLabelSymbol(cg);
         }
      /*
       * Generating the same instruction sequence as __sync_bool_compare_and_swap
       *
       * loop:
       *    ldxrx   resultReg, [addrReg]
       *    cmpx    resultReg, oldVReg
       *    bne     done
       *    stlxrx  resultReg, newVReg, [addrReg]
       *    cbnz    resultReg, loop
       *    dmb     ish
       * done:
       *    cset    resultReg, eq
       *
       */
      op = is64bit ? TR::InstOpCode::ldxrx : TR::InstOpCode::ldxrw;
      generateTrg1MemInstruction(cg, op, node, resultReg, new (cg->trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg));
      if (oldValueInReg)
         generateCompareInstruction(cg, node, resultReg, oldVReg, is64bit);
      else
         generateCompareImmInstruction(cg, node, resultReg, oldValue, is64bit);
      if (!createDoneLabel)
         generateTrg1ImmInstruction(cg, TR::InstOpCode::movzx, node, resultReg, 0); // failure
      generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, doneLabel, TR::CC_NE);

      if (casWithoutSync)
         {
         op = is64bit ? TR::InstOpCode::stxrx : TR::InstOpCode::stxrw;
         }
      else
         {
         op = is64bit ? TR::InstOpCode::stlxrx : TR::InstOpCode::stlxrw;
         }
      generateTrg1MemSrc1Instruction(cg, op, node, resultReg, new (cg->trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg), newVReg);
      generateCompareBranchInstruction(cg, TR::InstOpCode::cbnzx, node, resultReg, loopLabel);

      if (!casWithoutSync)
         generateSynchronizationInstruction(cg, TR::InstOpCode::dmb, node, 0xB); // dmb ish (Inner Shareable full barrier)

      if (createDoneLabel)
         {
         generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);
         generateCSetInstruction(cg, node, resultReg, TR::CC_EQ);
         }
      else
         {
         generateTrg1ImmInstruction(cg, TR::InstOpCode::movzx, node, resultReg, 1); // success
         }
      }
   srm->reclaimScratchRegister(addrReg);

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
   intptr_t offset;
   bool offsetInReg = true;

   if (thirdChild->getOpCode().isLoadConst() && thirdChild->getRegister() == NULL)
      {
      offset = (thirdChild->getOpCodeValue() == TR::iconst) ? thirdChild->getInt() : thirdChild->getLongInt();
      offsetInReg = !constantIsUnsignedImm12(offset);
      }
   if (offsetInReg)
      offsetReg = cg->evaluate(thirdChild);

   static const bool disableLSE = feGetEnv("TR_aarch64DisableLSE") != NULL;
   static const bool useLSE = comp->target().cpu.supportsFeature(OMR_FEATURE_ARM64_LSE) && (!disableLSE);
   // Obtain values to be checked for, and swapped in:
   if ((!useLSE) && fourthChild->getOpCode().isLoadConst() && fourthChild->getRegister() == NULL)
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
   TR_ARM64ScratchRegisterManager *srm = cg->generateScratchRegisterManager();

   // Compare and swap:
   resultReg = genCAS(node, cg, srm, objReg, offsetReg, offset, offsetInReg, oldVReg, newVReg, NULL, oldValue, oldValueInReg, isLong, casWithoutSync);

   const int regnum = 3 + (oldValueInReg ? 1 : 0) + (offsetInReg ? 1 : 0) + srm->numAvailableRegisters();
   conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, regnum, cg->trMemory());
   conditions->addPostCondition(objReg, TR::RealRegister::NoReg);
   if (offsetInReg)
      conditions->addPostCondition(offsetReg, TR::RealRegister::NoReg);
   conditions->addPostCondition(resultReg, TR::RealRegister::NoReg);
   conditions->addPostCondition(newVReg, TR::RealRegister::NoReg);

   if (oldValueInReg)
      conditions->addPostCondition(oldVReg, TR::RealRegister::NoReg);

   srm->addScratchRegistersToDependencyList(conditions);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

   srm->stopUsingRegisters();

   cg->recursivelyDecReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   if (offsetInReg)
      cg->decReferenceCount(thirdChild);
   else
      cg->recursivelyDecReferenceCount(thirdChild);

   if (oldValueInReg)
      cg->decReferenceCount(fourthChild);
   else
      cg->recursivelyDecReferenceCount(fourthChild);
   cg->decReferenceCount(fifthChild);
   return resultReg;
   }

static TR::Register *VMinlineCompareAndSwapObject(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = reinterpret_cast<TR_J9VMBase *>(comp->fe());
   TR::Register *objReg, *offsetReg, *resultReg;
   TR::Node *firstChild, *secondChild, *thirdChild, *fourthChild, *fifthChild;
   TR::LabelSymbol *doneLabel;
   bool offsetInReg = true;
   intptr_t offset;

   auto gcMode = TR::Compiler->om.writeBarrierType();
   const bool doWrtBar = (gcMode == gc_modron_wrtbar_oldcheck || gcMode == gc_modron_wrtbar_cardmark_and_oldcheck || gcMode == gc_modron_wrtbar_always);
   const bool doCrdMrk = (gcMode == gc_modron_wrtbar_cardmark || gcMode == gc_modron_wrtbar_cardmark_and_oldcheck || gcMode == gc_modron_wrtbar_cardmark_incremental);

   /**
    *  icall  jdk/internal/misc/Unsafe.compareAndSetObject
    *    aload  java/lang/invoke/VarHandle._unsafe
    *    aload  (objNode)
    *    lconst (offset)
    *    aload  (oldValueNode)
    *    aload  (newValueNode)
    */
   firstChild = node->getFirstChild();
   secondChild = node->getSecondChild();
   thirdChild = node->getChild(2);
   fourthChild = node->getChild(3);
   fifthChild = node->getChild(4);

   objReg = cg->evaluate(secondChild);

   if (thirdChild->getOpCode().isLoadConst() && thirdChild->getRegister() == NULL)
      {
      offset = (thirdChild->getOpCodeValue() == TR::iconst) ? thirdChild->getInt() : thirdChild->getLongInt();
      offsetInReg = !constantIsUnsignedImm12(offset);
      }
   if (offsetInReg)
      offsetReg = cg->evaluate(thirdChild);

   TR::Register *oldVReg = cg->evaluate(fourthChild);
   TR::Register *newVReg = cg->evaluate(fifthChild);
   doneLabel = generateLabelSymbol(cg);

   TR_ARM64ScratchRegisterManager *srm = cg->generateScratchRegisterManager();

#ifdef OMR_GC_CONCURRENT_SCAVENGER
   if (TR::Compiler->om.readBarrierType() != gc_modron_readbar_none)
      {
      TR::Register *tempReg = srm->findOrCreateScratchRegister();
      TR::Register *locationReg = cg->allocateRegister();
      TR::Register *evacuateReg = srm->findOrCreateScratchRegister();
      TR::Register *x0Reg = cg->allocateRegister();
      TR::Register *vmThreadReg = cg->getMethodMetaDataRegister();

      TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
      startLabel->setStartInternalControlFlow();
      endLabel->setEndInternalControlFlow();

      TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg->trMemory());
      deps->addPostCondition(tempReg, TR::RealRegister::NoReg);
      deps->addPostCondition(locationReg, TR::RealRegister::x1); // TR_softwareReadBarrier helper needs this in x1.
      deps->addPostCondition(evacuateReg, TR::RealRegister::NoReg);
      deps->addPostCondition(x0Reg, TR::RealRegister::x0);

      if (offsetInReg)
         {
         generateTrg1Src2Instruction(cg, TR::InstOpCode::addx, node, locationReg, objReg, offsetReg);
         }
      else
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addimmx, node, locationReg, objReg, offset);
         }
      TR::InstOpCode::Mnemonic loadOp = comp->useCompressedPointers() ? TR::InstOpCode::ldrimmw : TR::InstOpCode::ldrimmx;

      generateTrg1MemInstruction(cg, loadOp, node, tempReg, new (cg->trHeapMemory()) TR::MemoryReference(locationReg, 0, cg));

      if (node->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef())
         TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, tempReg);

      generateLabelInstruction(cg, TR::InstOpCode::label, node, startLabel);

      generateTrg1MemInstruction(cg, loadOp, node, evacuateReg,
            new (cg->trHeapMemory()) TR::MemoryReference(vmThreadReg, comp->fej9()->thisThreadGetEvacuateBaseAddressOffset(), cg));
      generateCompareInstruction(cg, node, tempReg, evacuateReg, true);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, endLabel, TR::CC_LT);

      generateTrg1MemInstruction(cg, loadOp, node, evacuateReg,
            new (cg->trHeapMemory()) TR::MemoryReference(vmThreadReg, comp->fej9()->thisThreadGetEvacuateTopAddressOffset(), cg));
      generateCompareInstruction(cg, node, tempReg, evacuateReg, true);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, endLabel, TR::CC_GT);

      // TR_softwareReadBarrier helper expects the vmThread in x0.
      generateMovInstruction(cg, node, x0Reg, vmThreadReg);

      TR::SymbolReference *helperSym = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_softwareReadBarrier);
      generateImmSymInstruction(cg, TR::InstOpCode::bl, node, (uintptr_t)helperSym->getMethodAddress(), deps, helperSym, NULL);

      generateLabelInstruction(cg, TR::InstOpCode::label, node, endLabel, deps);

      srm->reclaimScratchRegister(tempReg);
      srm->reclaimScratchRegister(evacuateReg);

      cg->stopUsingRegister(locationReg);
      cg->stopUsingRegister(x0Reg);

      cg->machine()->setLinkRegisterKilled(true);
      }
#endif //OMR_GC_CONCURRENT_SCAVENGER

   TR::Register *oReg = oldVReg;
   TR::Register *nReg = newVReg;
   bool useShiftedOffsets = (TR::Compiler->om.compressedReferenceShiftOffset() != 0);
   if (useShiftedOffsets)
      {
      if (!fourthChild->isNull())
         {
         oReg = srm->findOrCreateScratchRegister();
         generateLogicalShiftRightImmInstruction(cg, node, oReg, oldVReg, TR::Compiler->om.compressedReferenceShiftOffset());
         }
      if (!fifthChild->isNull())
         {
         nReg = srm->findOrCreateScratchRegister();
         generateLogicalShiftRightImmInstruction(cg, node, nReg, newVReg, TR::Compiler->om.compressedReferenceShiftOffset());
         }
      }
   resultReg = genCAS(node, cg, srm, objReg, offsetReg, offset, offsetInReg, oReg, nReg, doneLabel, 0, true, !comp->useCompressedPointers());

   if (useShiftedOffsets)
      {
      srm->reclaimScratchRegister(oReg);
      srm->reclaimScratchRegister(nReg);
      }

   const bool skipWrtBar = (gcMode == gc_modron_wrtbar_none) || (fifthChild->isNull() && (gcMode != gc_modron_wrtbar_always));
   if (!skipWrtBar)
      {
      TR::Register *wrtBarSrcReg = newVReg;

      if (objReg == wrtBarSrcReg)
         {
         // write barrier helper requires that dstObject and srcObject are in the different registers.
         // Because wrtBarSrcReg will be dead as soon as writeBarrier is done (which is not a GC safe point),
         // it is not required to be a collected refernce register.
         wrtBarSrcReg = srm->findOrCreateScratchRegister();
         generateMovInstruction(cg, node, wrtBarSrcReg, objReg, true);
         }

      const bool srcNonNull = fifthChild->isNonNull();

      if (doWrtBar) // generational or gencon
         {
         TR::SymbolReference *wbRef = (gcMode == gc_modron_wrtbar_always) ?
            comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef() :
            // use jitWriteBarrierStoreGenerational for both generational and gencon, because we inline card marking.
            comp->getSymRefTab()->findOrCreateWriteBarrierStoreGenerationalSymbolRef();

         if (!srcNonNull)
            {
            // If object is NULL, done
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:000srcNullChk"), *srm);
            generateCompareBranchInstruction(cg, TR::InstOpCode::cbzx, node, wrtBarSrcReg, doneLabel);
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:000srcNullChk:NonNull"), *srm);
            }
         // Inlines cardmarking and remembered bit check for gencon.
         VMnonNullSrcWrtBarCardCheckEvaluator(node, objReg, wrtBarSrcReg, srm, doneLabel, wbRef, cg);

         }
      else if (doCrdMrk)
         {
         TR::SymbolReference *wbRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef();
         if (!srcNonNull)
            {
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:000srcNullChk"), *srm);
            generateCompareBranchInstruction(cg, TR::InstOpCode::cbzx, node, wrtBarSrcReg, doneLabel);
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "wrtbarEvaluator:000srcNullChk:NonNull"), *srm);
            }
         VMCardCheckEvaluator(node, objReg, srm, doneLabel, cg);
         }

      TR_ARM64ScratchRegisterDependencyConditions scratchDeps;
      scratchDeps.addDependency(cg, objReg, doWrtBar ? TR::RealRegister::x0 : TR::RealRegister::NoReg);
      scratchDeps.addDependency(cg, wrtBarSrcReg, doWrtBar ? TR::RealRegister::x1 : TR::RealRegister::NoReg);
      if (offsetInReg)
         {
         scratchDeps.addDependency(cg, offsetReg, TR::RealRegister::NoReg);
         }
      scratchDeps.unionDependency(cg, oldVReg, TR::RealRegister::NoReg);
      scratchDeps.unionDependency(cg, newVReg, TR::RealRegister::NoReg);
      scratchDeps.addDependency(cg, resultReg, TR::RealRegister::NoReg);
      scratchDeps.addScratchRegisters(cg, srm);
      TR::RegisterDependencyConditions *conditions = TR_ARM64ScratchRegisterDependencyConditions::createDependencyConditions(cg, NULL, &scratchDeps);

      generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions, NULL);
      }
   else
      {
      TR_ARM64ScratchRegisterDependencyConditions scratchDeps;
      scratchDeps.addDependency(cg, objReg,  TR::RealRegister::NoReg);
      if (offsetInReg)
         {
         scratchDeps.addDependency(cg, offsetReg, TR::RealRegister::NoReg);
         }
      scratchDeps.unionDependency(cg, oldVReg, TR::RealRegister::NoReg);
      scratchDeps.unionDependency(cg, newVReg, TR::RealRegister::NoReg);
      scratchDeps.addDependency(cg, resultReg, TR::RealRegister::NoReg);
      scratchDeps.addScratchRegisters(cg, srm);
      TR::RegisterDependencyConditions *conditions = TR_ARM64ScratchRegisterDependencyConditions::createDependencyConditions(cg, NULL, &scratchDeps);

      generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions, NULL);
      }

   srm->stopUsingRegisters();

   cg->recursivelyDecReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   if (offsetInReg)
      {
      cg->decReferenceCount(thirdChild);
      }
   else
      {
      cg->recursivelyDecReferenceCount(thirdChild);
      }

   cg->decReferenceCount(fourthChild);
   cg->decReferenceCount(fifthChild);

   return resultReg;
   }

bool
J9::ARM64::CodeGenerator::inlineDirectCall(TR::Node *node, TR::Register *&resultReg)
   {
   TR::CodeGenerator *cg = self();
   TR::MethodSymbol * methodSymbol = node->getSymbol()->getMethodSymbol();

   if (OMR::CodeGeneratorConnector::inlineDirectCall(node, resultReg))
      {
      return true;
      }
   if (methodSymbol)
      {
      switch (methodSymbol->getRecognizedMethod())
         {
         case TR::java_nio_Bits_keepAlive:
         case TR::java_lang_ref_Reference_reachabilityFence:
            {

            // The only purpose of these functions is to prevent an otherwise
            // unreachable object from being garbage collected, because we don't
            // want its finalizer to be called too early.  There's no need to
            // generate a full-blown call site just for this purpose.  

            TR::Node *paramNode = node->getFirstChild();
            TR::Register *paramReg = cg->evaluate(paramNode);

            // In theory, a value could be kept alive on the stack, rather than in
            // a register.  It is unfortunate that the following deps will force
            // the value into a register for no reason.  However, in many common
            // cases, this label will have no effect on the generated code, and
            // will only affect GC maps.
            //
            TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());
            TR::addDependency(conditions, paramReg, TR::RealRegister::NoReg, TR_GPR, cg);
            TR::LabelSymbol *label = generateLabelSymbol(cg);
            generateLabelInstruction(cg, TR::InstOpCode::label, node, label, conditions);
            cg->decReferenceCount(paramNode);
            resultReg = NULL;
            return true;
            }

         case TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z:
            {
            // In Java9 and newer this can be either the jdk.internal JNI method or the sun.misc Java wrapper.
            // In Java8 it will be sun.misc which will contain the JNI directly.
            // We only want to inline the JNI methods, so add an explicit test for isNative().
            if (!methodSymbol->isNative())
               break;

            if ((node->isUnsafeGetPutCASCallOnNonArray() || !TR::Compiler->om.canGenerateArraylets()) && node->isSafeForCGToFastPathUnsafeCall())
               {
               resultReg = VMinlineCompareAndSwap(node, cg, false);
               return true;
               }
            break;
            }

         case TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z:
            {
            // As above, we only want to inline the JNI methods, so add an explicit test for isNative()
            if (!methodSymbol->isNative())
               break;

            if ((node->isUnsafeGetPutCASCallOnNonArray() || !TR::Compiler->om.canGenerateArraylets()) && node->isSafeForCGToFastPathUnsafeCall())
               {
               resultReg = VMinlineCompareAndSwap(node, cg, true);
               return true;
               }
            break;
            }

         case TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z:
            {
            if (!methodSymbol->isNative())
               break;

            if ((node->isUnsafeGetPutCASCallOnNonArray() || !TR::Compiler->om.canGenerateArraylets()) && node->isSafeForCGToFastPathUnsafeCall())
               {
               resultReg = VMinlineCompareAndSwapObject(node, cg);
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
   // NOTE:
   // If no code is generated for the null check, just evaluate the
   // child and decrement its use count UNLESS the child is a pass-through node
   // in which case some kind of explicit test or indirect load must be generated
   // to force the null check at this point.
   TR::Node * const firstChild = node->getFirstChild();
   TR::ILOpCode &opCode = firstChild->getOpCode();
   TR::Node *reference = NULL;
   TR::Compilation *comp = cg->comp();
   TR::Node *n = firstChild;
   bool hasCompressedPointers = false;

   // NULLCHK has a special case with compressed pointers.
   // In the scenario where the first child is TR::l2a, the
   // node to be null checked is not the iloadi, but its child.
   // i.e. aload, aRegLoad, etc.
   if (comp->useCompressedPointers() && firstChild->getOpCodeValue() == TR::l2a)
      {
      // pattern match the sequence under the l2a
      // NULLCHK        NULLCHK                     <- node
      //    aloadi f      l2a
      //       aload O      lshl
      //                       iu2l
      //                          iloadi/irdbari f <- n
      //                             aload O        <- reference
      //                       iconst shftKonst
      //
      hasCompressedPointers = true;
      TR::ILOpCodes loadOp = cg->comp()->il.opCodeForIndirectLoad(TR::Int32);
      TR::ILOpCodes rdbarOp = cg->comp()->il.opCodeForIndirectReadBarrier(TR::Int32);
      while ((n->getOpCodeValue() != loadOp) && (n->getOpCodeValue() != rdbarOp))
         n = n->getFirstChild();
      reference = n->getFirstChild();
      }
   else
      reference = node->getNullCheckReference();

   // Skip the NULLCHK for TR::loadaddr nodes.
   //
   if (cg->getHasResumableTrapHandler()
         && reference->getOpCodeValue() == TR::loadaddr)
      {
      cg->evaluate(firstChild);
      cg->decReferenceCount(firstChild);
      return NULL;
      }

   bool needExplicitCheck  = true;
   bool needLateEvaluation = true;
   bool firstChildEvaluated = false;

   // Add the explicit check after this instruction
   //
   TR::Instruction *appendTo = NULL;

   // determine if an explicit check is needed
   if (cg->getHasResumableTrapHandler())
      {
      if (n->getOpCode().isLoadVar()
            || (opCode.getOpCodeValue() == TR::l2i))
         {
         TR::SymbolReference *symRef = NULL;

         if (opCode.getOpCodeValue() == TR::l2i)
            symRef = n->getFirstChild()->getSymbolReference();
         else
            symRef = n->getSymbolReference();

         // We prefer to generate an explicit NULLCHK vs an implicit one
         // to prevent potential costs of a cache miss on an unnecessary load.
         if (n->getReferenceCount() == 1
               && !n->getSymbolReference()->isUnresolved())
            {
            // If the child is only used here, we don't need to evaluate it
            // since all we need is the grandchild which will be evaluated by
            // the generation of the explicit check below.
            needLateEvaluation = false;

            // at this point, n is the raw iloadi (created by lowerTrees) and
            // reference is the aload of the object. node->getFirstChild is the
            // l2a sequence; as a result, n's refCount will always be 1.
            //
            if (hasCompressedPointers
                  && node->getFirstChild()->getReferenceCount() >= 2)
               {
               // In this case, the result of load is used in other places, so we need to evaluate it here
               //
               needLateEvaluation = true;

               // Check if offset from a NULL reference will fall into the inaccessible bytes,
               // resulting in an implicit trap being raised.
               if (symRef
                   && ((symRef->getSymbol()->getOffset() + symRef->getOffset()) < cg->getNumberBytesReadInaccessible()))
                  {
                  needExplicitCheck = false;
                  }
               }
            }

         // Check if offset from a NULL reference will fall into the inaccessible bytes,
         // resulting in an implicit trap being raised.
         else if (symRef
                  && ((symRef->getSymbol()->getOffset() + symRef->getOffset()) < cg->getNumberBytesReadInaccessible()))
            {
            needExplicitCheck = false;

            // If the child is an arraylength which has been reduced to an iiload,
            // and is only going to be used immediately in a BNDCHK, combine the checks.
            //
            TR::TreeTop *nextTreeTop = cg->getCurrentEvaluationTreeTop()->getNextTreeTop();
            if (n->getReferenceCount() == 2 && nextTreeTop)
               {
               TR::Node *nextTopNode = nextTreeTop->getNode();

               if (nextTopNode)
                  {
                  if (nextTopNode->getOpCode().isBndCheck())
                     {
                     if ((nextTopNode->getOpCode().isSpineCheck() && (nextTopNode->getChild(2) == n))
                           || (!nextTopNode->getOpCode().isSpineCheck() && (nextTopNode->getFirstChild() == n)))
                        {
                        needLateEvaluation = false;
                        nextTopNode->setHasFoldedImplicitNULLCHK(true);
                        if (comp->getOption(TR_TraceCG))
                           {
                           traceMsg(comp, "\nMerging NULLCHK [%p] and BNDCHK [%p] of load child [%p]\n", node, nextTopNode, n);
                           }
                        }
                     }
                  else if (nextTopNode->getOpCode().isIf()
                        && nextTopNode->isNonoverriddenGuard()
                        && nextTopNode->getFirstChild() == firstChild)
                     {
                     needLateEvaluation = false;
                     needExplicitCheck = true;
                     }
                  }
               }
            }
         }
      else if (opCode.isStore())
         {
         TR::SymbolReference *symRef = n->getSymbolReference();
         if (n->getOpCode().hasSymbolReference()
               && (symRef->getSymbol()->getOffset() + symRef->getOffset() < cg->getNumberBytesWriteInaccessible()))
            {
            needExplicitCheck = false;
            }
         }
      else if (opCode.isCall()
            && opCode.isIndirect()
            && (cg->getNumberBytesReadInaccessible() > TR::Compiler->om.offsetOfObjectVftField()))
         {
         TR::SymbolReference *methodSymRef = firstChild->getSymbolReference();
         TR::MethodSymbol *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
         // Currently, we do not have instructions to access receiver for interface calls in main line code.
         // Thus, we need to do null check explictly.
         if (!methodSymbol->isInterface())
            {
            needExplicitCheck = false;
            }
         else
            {
            if (comp->getOption(TR_TraceCG))
               {
               traceMsg(comp, "\nExcplicit NULLCHK on interface call [%p]\n", firstChild);
               }
            }
         }
      else if (opCode.getOpCodeValue() == TR::iushr
            && (cg->getNumberBytesReadInaccessible() > cg->fe()->getOffsetOfContiguousArraySizeField()))
         {
         // If the child is an arraylength which has been reduced to an iushr,
         // we must evaluate it here so that the implicit exception will occur
         // at the right point in the program.
         //
         // This can occur when the array length is represented in bytes, not elements.
         // The optimizer must intervene for this to happen.
         //
         cg->evaluate(n->getFirstChild());
         needExplicitCheck = false;
         }
      else if (opCode.getOpCodeValue() == TR::monent
            || opCode.getOpCodeValue() == TR::monexit)
         {
         // The child may generate inline code that provides an implicit null check
         // but we won't know until the child is evaluated.
         //
         needLateEvaluation = false;
         cg->evaluate(reference);
         appendTo = cg->getAppendInstruction();
         cg->evaluate(firstChild);
         firstChildEvaluated = true;
         if (cg->getImplicitExceptionPoint()
               && (cg->getNumberBytesReadInaccessible() > cg->fe()->getOffsetOfContiguousArraySizeField()))
            {
            needExplicitCheck = false;
            }
         }
      }

   // Generate the code for the null check
   //
   if(needExplicitCheck)
      {
      TR::Register * targetRegister = NULL;
      /* TODO: Resolution */
      /* if(needsResolve) ... */

      TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
      TR::Snippet *snippet = new (cg->trHeapMemory()) TR::ARM64HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference(), NULL);
      cg->addSnippet(snippet);
      TR::Register *referenceReg = cg->evaluate(reference);
      TR::Instruction *cbzInstruction = generateCompareBranchInstruction(cg, TR::InstOpCode::cbzx, node, referenceReg, snippetLabel, appendTo);
      cbzInstruction->setNeedsGCMap(0xffffffff);
      snippet->gcMap().setGCRegisterMask(0xffffffff);
      // ARM64HelperCallSnippet generates "bl" instruction
      cg->machine()->setLinkRegisterKilled(true);
      }

   // If we need to evaluate the child, do so. Otherwise, if we have
   // evaluated the reference node, then decrement its use count.
   // The use count of the child is decremented when we are done
   // evaluating the NULLCHK.
   //
   if (needLateEvaluation)
      {
      cg->evaluate(firstChild);
      firstChildEvaluated = true;
      }
   // If the firstChild is evaluated, we simply call decReferenceCount.
   // Otherwise, we need to call recursivelyDecReferenceCount so that the ref count of
   // child nodes of the firstChild is properly decremented when the ref count of the firstChild is 1.
   if (firstChildEvaluated)
      {
      cg->decReferenceCount(firstChild);
      }
   else
      {
      cg->recursivelyDecReferenceCount(firstChild);
      }

   // If an explicit check has not been generated for the null check, there is
   // an instruction that will cause a hardware trap if the exception is to be
   // taken. If this method may catch the exception, a GC stack map must be
   // created for this instruction. All registers are valid at this GC point
   // TODO - if the method may not catch the exception we still need to note
   // that the GC point exists, since maps before this point and after it cannot
   // be merged.
   //
   if (cg->getHasResumableTrapHandler() && !needExplicitCheck)
      {
      TR::Instruction *faultingInstruction = cg->getImplicitExceptionPoint();
      if (faultingInstruction)
         {
         faultingInstruction->setNeedsGCMap(0xffffffff);
         cg->machine()->setLinkRegisterKilled(true);

         TR_Debug * debugObj = cg->getDebug();
         if (debugObj)
            {
            debugObj->addInstructionComment(faultingInstruction, "Throws Implicit Null Pointer Exception");
            }
         }
      }

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

   return NULL;
   }

static void
genBoundCheck(TR::CodeGenerator *cg, TR::Node *node, TR::Register *indexReg, int32_t indexVal, TR::Register *arrayLengthReg, int32_t arrayLengthVal)
   {
   TR::Instruction *gcPoint;

   TR::LabelSymbol *boundCheckFailSnippetLabel = cg->lookUpSnippet(TR::Snippet::IsHelperCall, node->getSymbolReference());
   if (!boundCheckFailSnippetLabel)
      {
      boundCheckFailSnippetLabel = generateLabelSymbol(cg);
      cg->addSnippet(new (cg->trHeapMemory()) TR::ARM64HelperCallSnippet(cg, node, boundCheckFailSnippetLabel, node->getSymbolReference()));
      }

   if (indexReg)
      generateCompareInstruction(cg, node, arrayLengthReg, indexReg, false); // 32-bit compare
   else
      generateCompareImmInstruction(cg, node, arrayLengthReg, indexVal, false); // 32-bit compare

   gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, boundCheckFailSnippetLabel, TR::CC_LS);

   // Exception edges don't have any live regs
   gcPoint->ARM64NeedsGCMap(cg, 0);

   // ARM64HelperCallSnippet generates "bl" instruction
   cg->machine()->setLinkRegisterKilled(true);
   }

static TR::Instruction *
genSpineCheck(TR::CodeGenerator *cg, TR::Node *node, TR::Register *arrayLengthReg, TR::LabelSymbol *discontiguousArrayLabel)
   {
   return generateCompareBranchInstruction(cg, TR::InstOpCode::cbzw, node, arrayLengthReg, discontiguousArrayLabel);
   }

static TR::Instruction *
genSpineCheck(TR::CodeGenerator *cg, TR::Node *node, TR::Register *baseArrayReg, TR::Register *arrayLengthReg, TR::LabelSymbol *discontiguousArrayLabel)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR::MemoryReference *contiguousArraySizeMR = new (cg->trHeapMemory()) TR::MemoryReference(baseArrayReg, fej9->getOffsetOfContiguousArraySizeField(), cg);
   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmw, node, arrayLengthReg, contiguousArraySizeMR);
   return genSpineCheck(cg, node, arrayLengthReg, discontiguousArrayLabel);
   }

static void
genArrayletAccessAddr(TR::CodeGenerator *cg, TR::Node *node, int32_t elementSize,
      // Inputs:
      TR::Register *baseArrayReg, TR::Register *indexReg, int32_t indexVal,
      // Outputs:
      TR::Register *arrayletReg, TR::Register *offsetReg, int32_t& offsetVal)
   {
   TR::Compilation* comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   TR_ASSERT(offsetReg || !indexReg, "Expecting valid offset reg when index reg is passed");

   uintptr_t arrayHeaderSize = TR::Compiler->om.discontiguousArrayHeaderSizeInBytes();
   int32_t spinePointerSize = TR::Compiler->om.sizeofReferenceField();
   int32_t spinePointerSizeShift = spinePointerSize == 8 ? 3 : 2;

   TR::MemoryReference *spineMR;
   TR::InstOpCode::Mnemonic loadOp;

   // Calculate the spine offset.
   //
   if (indexReg)
      {
      int32_t spineShift = fej9->getArraySpineShift(elementSize);

      // spineOffset = (index >> spineShift) * spinePtrSize
      //             = (index >> spineShift) << spinePtrSizeShift
      // spineOffset += arrayHeaderSize
      //
      TR_ASSERT(spineShift >= spinePointerSizeShift, "Unexpected spine shift value");
      generateLogicalShiftRightImmInstruction(cg, node, arrayletReg, indexReg, spineShift);
      generateLogicalShiftLeftImmInstruction(cg, node, arrayletReg, arrayletReg, spinePointerSizeShift);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addimmx, node, arrayletReg, arrayletReg, arrayHeaderSize);

      spineMR = new (cg->trHeapMemory()) TR::MemoryReference(baseArrayReg, arrayletReg, cg);
      loadOp = spinePointerSize == 8 ? TR::InstOpCode::ldroffx : TR::InstOpCode::ldroffw;
      }
   else
      {
      int32_t spineIndex = fej9->getArrayletLeafIndex(indexVal, elementSize);
      int32_t spineDisp32 = spineIndex * spinePointerSize + arrayHeaderSize;

      spineMR = new (cg->trHeapMemory()) TR::MemoryReference(baseArrayReg, spineDisp32, cg);
      loadOp = spinePointerSize == 8 ? TR::InstOpCode::ldrimmx : TR::InstOpCode::ldrimmw;
      }

   // Load the arraylet from the spine.
   //
   generateTrg1MemInstruction(cg, loadOp, node, arrayletReg, spineMR);

   // Calculate the arraylet offset.
   //
   if (indexReg)
      {
      int32_t arrayletMask = fej9->getArrayletMask(elementSize);

      loadConstant64(cg, node, arrayletMask, offsetReg);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::andx, node, offsetReg, indexReg, offsetReg);
      if (elementSize > 1)
         {
         int32_t elementSizeShift = CHAR_BIT * sizeof(int32_t) - leadingZeroes(elementSize - 1);
         generateLogicalShiftLeftImmInstruction(cg, node, offsetReg, offsetReg, elementSizeShift);
         }
      }
   else
      offsetVal = (fej9->getLeafElementIndex(indexVal, elementSize) * elementSize);
   }

static void
genDecompressPointer(TR::CodeGenerator *cg, TR::Node *node, TR::Register *ptrReg)
   {
   int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();

   if (shiftAmount != 0)
      generateLogicalShiftLeftImmInstruction(cg, node, ptrReg, ptrReg, shiftAmount);
   }

static TR::InstOpCode::Mnemonic
getLoadOpCodeFromDataType(TR::CodeGenerator *cg, TR::DataType dt, int32_t elementSize, bool isUnsigned, bool useIdxReg)
   {
   switch (dt)
      {
      case TR::Int8:
         if (isUnsigned)
            return useIdxReg ? TR::InstOpCode::ldrboff : TR::InstOpCode::ldrbimm;
         else
            return useIdxReg ? TR::InstOpCode::ldrsboffw : TR::InstOpCode::ldrsbimmw;
      case TR::Int16:
         if (isUnsigned)
            return useIdxReg ? TR::InstOpCode::ldrhoff : TR::InstOpCode::ldrhimm;
         else
            return useIdxReg ? TR::InstOpCode::ldrshoffw : TR::InstOpCode::ldrshimmw;
      case TR::Int32:
         return useIdxReg ? TR::InstOpCode::ldroffw : TR::InstOpCode::ldrimmw;
      case TR::Int64:
         return useIdxReg ? TR::InstOpCode::ldroffx : TR::InstOpCode::ldrimmx;
      case TR::Float:
         return useIdxReg ? TR::InstOpCode::vldroffs : TR::InstOpCode::vstrimms;
      case TR::Double:
         return useIdxReg ? TR::InstOpCode::vldroffd : TR::InstOpCode::vstrimmd;
      case TR::Address:
         if (elementSize == 8)
            return useIdxReg ? TR::InstOpCode::ldroffx : TR::InstOpCode::ldrimmx;
         else
            return useIdxReg ? TR::InstOpCode::ldroffw : TR::InstOpCode::ldrimmw;
      default:
         TR_ASSERT(false, "Unexpected array data type");
         return TR::InstOpCode::bad;
      }
   }

static TR::InstOpCode::Mnemonic
getStoreOpCodeFromDataType(TR::CodeGenerator *cg, TR::DataType dt, int32_t elementSize, bool useIdxReg)
   {
   switch (dt)
      {
      case TR::Int8:
         return useIdxReg ? TR::InstOpCode::strboff : TR::InstOpCode::strbimm;
      case TR::Int16:
         return useIdxReg ? TR::InstOpCode::strhoff : TR::InstOpCode::strhimm;
      case TR::Int32:
         return useIdxReg ? TR::InstOpCode::stroffw : TR::InstOpCode::strimmw;
      case TR::Int64:
         return useIdxReg ? TR::InstOpCode::stroffx : TR::InstOpCode::strimmx;
      case TR::Float:
         return useIdxReg ? TR::InstOpCode::vstroffs : TR::InstOpCode::vstrimms;
      case TR::Double:
         return useIdxReg ? TR::InstOpCode::vstroffd : TR::InstOpCode::vstrimmd;
      case TR::Address:
         if (elementSize == 8)
            return useIdxReg ? TR::InstOpCode::stroffx : TR::InstOpCode::strimmx;
         else
            return useIdxReg ? TR::InstOpCode::stroffw : TR::InstOpCode::strimmw;
      default:
         TR_ASSERT(false, "Unexpected array data type");
         return TR::InstOpCode::bad;
      }
   }

// Handles both BNDCHKwithSpineCHK and SpineCHK nodes.
//
TR::Register *
J9::ARM64::TreeEvaluator::BNDCHKwithSpineCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   bool needsBoundCheck = node->getOpCodeValue() == TR::BNDCHKwithSpineCHK;
   bool needsBoundCheckOOL;

   TR_ARM64ScratchRegisterManager *srm = cg->generateScratchRegisterManager();

   TR::Node *loadOrStoreChild = node->getFirstChild();
   TR::Node *baseArrayChild = node->getSecondChild();
   TR::Node *arrayLengthChild;
   TR::Node *indexChild;

   if (needsBoundCheck)
      {
      arrayLengthChild = node->getChild(2);
      indexChild = node->getChild(3);
      }
   else
      indexChild = node->getChild(2);

   TR::Register *baseArrayReg = cg->evaluate(baseArrayChild);
   TR::Register *indexReg;
   TR::Register *loadOrStoreReg;
   TR::Register *arrayLengthReg;

   // If the index is too large to be an immediate load it in a register
   if (!indexChild->getOpCode().isLoadConst() || !constantIsUnsignedImm12(indexChild->getInt()))
      indexReg = cg->evaluate(indexChild);
   else
      indexReg = NULL;

   // For primitive stores anchored under the check node, we must evaluate the source node
   // before the bound check branch so that its available to the snippet.
   //
   if (loadOrStoreChild->getOpCode().isStore() && !loadOrStoreChild->getRegister())
      {
      TR::Node *valueChild = loadOrStoreChild->getSecondChild();
      cg->evaluate(valueChild);
      }

   // Evaluate any escaping nodes before the OOL branch since they won't be evaluated in the OOL path.
   preEvaluateEscapingNodesForSpineCheck(node, cg);

   // Label to the OOL code that will perform the load/store/agen for discontiguous arrays (and the bound check if needed).
   TR::LabelSymbol *discontiguousArrayLabel = generateLabelSymbol(cg);

   // Label back to main-line that the OOL code will branch to when done.
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   doneLabel->setEndInternalControlFlow();

   TR_ARM64OutOfLineCodeSection *discontiguousArrayOOL = new (cg->trHeapMemory()) TR_ARM64OutOfLineCodeSection(discontiguousArrayLabel, doneLabel, cg);
   cg->getARM64OutOfLineCodeSectionList().push_front(discontiguousArrayOOL);

   TR::Instruction *OOLBranchInstr;

   if (needsBoundCheck)
      {
      TR_ASSERT(arrayLengthChild, "Expecting to have an array length child for BNDCHKwithSpineCHK node");
      TR_ASSERT(
            arrayLengthChild->getOpCode().isConversion() || arrayLengthChild->getOpCodeValue() == TR::iloadi || arrayLengthChild->getOpCodeValue() == TR::iload
                  || arrayLengthChild->getOpCodeValue() == TR::iRegLoad || arrayLengthChild->getOpCode().isLoadConst(),
            "Expecting array length child under BNDCHKwithSpineCHK to be a conversion, iiload, iload, iRegLoad or iconst");

      arrayLengthReg = arrayLengthChild->getRegister();

      if (arrayLengthReg)
         {
         OOLBranchInstr = genSpineCheck(cg, node, baseArrayReg, arrayLengthReg, discontiguousArrayLabel);
         needsBoundCheckOOL = true;
         genBoundCheck(cg, node, indexReg, indexChild->getInt(), arrayLengthReg, arrayLengthChild->getInt());
         }
      else if (arrayLengthChild->getOpCode().isLoadConst())
         {
         // If the constant arraylength is non-zero then it will pass the spine check (hence its
         // a contiguous array) and the BNDCHK can be done inline with no OOL path.
         //
         // If the constant arraylength is zero then we will always go OOL.
         //
         // TODO: in the future there shouldn't be an OOL path because any valid access must be
         //       on a discontiguous array.
         //
         if (arrayLengthChild->getInt() != 0)
            {
            // The array must be contiguous.
            //

            // If the array length is too large to be an immediate load it in a register for the bound check
            if (!constantIsUnsignedImm12(arrayLengthChild->getInt()))
               arrayLengthReg = cg->evaluate(arrayLengthChild);

            // Do the bound check first.
            genBoundCheck(cg, node, indexReg, indexChild->getInt(), arrayLengthReg, arrayLengthChild->getInt());
            needsBoundCheckOOL = false;
            TR::Register *scratchArrayLengthReg = srm->findOrCreateScratchRegister();
            OOLBranchInstr = genSpineCheck(cg, node, baseArrayReg, scratchArrayLengthReg, discontiguousArrayLabel);
            srm->reclaimScratchRegister(scratchArrayLengthReg);
            }
         else
            {
            // Zero length array or discontiguous array.  Unconditionally branch to the OOL path
            // to find out which.
            //
            OOLBranchInstr = generateLabelInstruction(cg, TR::InstOpCode::b, node, discontiguousArrayLabel);
            needsBoundCheckOOL = true;
            }
         }
      else
         {
         // Load the contiguous array length.
         arrayLengthReg = cg->evaluate(arrayLengthChild);
         // If the array length is 0, this is a discontiguous array and the bound check will be handled OOL.
         OOLBranchInstr = genSpineCheck(cg, node, arrayLengthReg, discontiguousArrayLabel);
         needsBoundCheckOOL = true;
         // Do the bound check using the contiguous array length.
         genBoundCheck(cg, node, indexReg, indexChild->getInt(), arrayLengthReg, arrayLengthChild->getInt());
         }

      cg->decReferenceCount(arrayLengthChild);
      }
   else
      {
      // Spine check only; load the contiguous length, check for 0, branch OOL if discontiguous.
      needsBoundCheckOOL = false;

      arrayLengthReg = srm->findOrCreateScratchRegister();
      OOLBranchInstr = genSpineCheck(cg, node, baseArrayReg, arrayLengthReg, discontiguousArrayLabel);
      srm->reclaimScratchRegister(arrayLengthReg);
      }

   // For reference stores, only evaluate the array element address because the store cannot
   // happen here (it must be done via the array store check).
   //
   // For primitive stores, evaluate them now.
   // For loads, evaluate them now.
   // For address calculations (aladd/aiadd), evaluate them now.
   //
   bool doLoadOrStore;
   bool doLoadDecompress = false;
   bool doAddressComputation;

   if (loadOrStoreChild->getOpCode().isStore() && loadOrStoreChild->getReferenceCount() > 1)
      {
      TR_ASSERT(loadOrStoreChild->getOpCode().isWrtBar(), "Opcode must be wrtbar");
      loadOrStoreReg = cg->evaluate(loadOrStoreChild->getFirstChild());
      cg->decReferenceCount(loadOrStoreChild->getFirstChild());
      doLoadOrStore = false;
      doAddressComputation = true;
      }
   else
      {
      // If it's a store and not commoned then it must be a primitive store.
      // If it's an address load it may need decompression in the OOL path.

      // Top-level check whether a decompression sequence is necessary, because the first child
      // may have been created by a PRE temp.
      //
      if ((loadOrStoreChild->getOpCodeValue() == TR::aload || loadOrStoreChild->getOpCodeValue() == TR::aRegLoad)
          && node->isSpineCheckWithArrayElementChild()
          && comp->useCompressedPointers())
         {
         doLoadDecompress = true;
         }

      TR::Node *actualLoadOrStoreChild = loadOrStoreChild;
      while (actualLoadOrStoreChild->getOpCode().isConversion() || actualLoadOrStoreChild->containsCompressionSequence())
         {
         if (actualLoadOrStoreChild->containsCompressionSequence())
            doLoadDecompress = true;
         actualLoadOrStoreChild = actualLoadOrStoreChild->getFirstChild();
         }

      doLoadOrStore = actualLoadOrStoreChild->getOpCode().hasSymbolReference()
            && (actualLoadOrStoreChild->getSymbolReference()->getSymbol()->isArrayShadowSymbol()
                  || actualLoadOrStoreChild->getSymbolReference()->getSymbol()->isArrayletShadowSymbol()) && node->isSpineCheckWithArrayElementChild();

      // If the 1st child is not a load/store/aladd/aiadd it's probably a nop (e.g. const) at this point due to commoning
      //
      doAddressComputation = !doLoadOrStore && actualLoadOrStoreChild->getOpCode().isArrayRef() && !node->isSpineCheckWithArrayElementChild();

      if (doLoadOrStore || doAddressComputation || !loadOrStoreChild->getOpCode().isLoadConst())
         loadOrStoreReg = cg->evaluate(loadOrStoreChild);
      else
         loadOrStoreReg = NULL;
      }

   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);
   TR::LabelSymbol *doneMainlineLabel = generateLabelSymbol(cg);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneMainlineLabel);

   // start of OOL
   //
   discontiguousArrayOOL->swapInstructionListsWithCompilation();
      {
      TR::Instruction *OOLLabelInstr = generateLabelInstruction(cg, TR::InstOpCode::label, node, discontiguousArrayLabel);
      // XXX: Temporary fix, OOL instruction stream does not pick up live locals or monitors correctly.
      TR_ASSERT(!OOLLabelInstr->getLiveLocals() && !OOLLabelInstr->getLiveMonitors(), "Expecting first OOL instruction to not have live locals/monitors info");
      OOLLabelInstr->setLiveLocals(OOLBranchInstr->getLiveLocals());
      OOLLabelInstr->setLiveMonitors(OOLBranchInstr->getLiveMonitors());

      if (needsBoundCheckOOL)
         {
         TR_ASSERT(needsBoundCheck, "Inconsistent state, needs bound check OOL but doesn't need bound check");

         TR::MemoryReference *discontiguousArraySizeMR = new (cg->trHeapMemory()) TR::MemoryReference(baseArrayReg, fej9->getOffsetOfDiscontiguousArraySizeField(), cg);
         TR::Register *arrayLengthScratchReg = srm->findOrCreateScratchRegister();

         generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmw, node, arrayLengthScratchReg, discontiguousArraySizeMR);

         // Do the bound check using the discontiguous array length.
         genBoundCheck(cg, node, indexReg, indexChild->getInt(), arrayLengthScratchReg, arrayLengthChild->getInt());

         srm->reclaimScratchRegister(arrayLengthScratchReg);
         }

      TR_ASSERT(!(doLoadOrStore && doAddressComputation), "Unexpected condition");

      TR::Register *arrayletReg = NULL;
      TR::DataType dt = loadOrStoreChild->getDataType();

      if (doLoadOrStore || doAddressComputation)
         {
         arrayletReg = doAddressComputation ? loadOrStoreReg : cg->allocateRegister();

         // Generate the base+offset address pair into the arraylet.
         //
         int32_t elementSize = (dt == TR::Address) ? TR::Compiler->om.sizeofReferenceField() : TR::Symbol::convertTypeToSize(dt);
         TR::Register *arrayletOffsetReg;
         int32_t arrayletOffsetVal;

         if (indexReg)
            arrayletOffsetReg = srm->findOrCreateScratchRegister();

         genArrayletAccessAddr(cg, node, elementSize, baseArrayReg, indexReg, indexChild->getInt(), arrayletReg, arrayletOffsetReg, arrayletOffsetVal);

         // Decompress the arraylet pointer if necessary.
         //
         genDecompressPointer(cg, node, arrayletReg);

         if (doLoadOrStore)
            {
            // Generate the load or store.
            //
            if (loadOrStoreChild->getOpCode().isStore())
               {
               TR::InstOpCode::Mnemonic storeOp = getStoreOpCodeFromDataType(cg, dt, elementSize, indexReg != NULL);

               TR::MemoryReference *arrayletMR = indexReg ?
                  new (cg->trHeapMemory()) TR::MemoryReference(arrayletReg, arrayletOffsetReg, cg) :
                  new (cg->trHeapMemory()) TR::MemoryReference(arrayletReg, arrayletOffsetVal, cg);
               generateMemSrc1Instruction(cg, storeOp, node, arrayletMR, loadOrStoreChild->getSecondChild()->getRegister());
               }
            else
               {
               TR_ASSERT(loadOrStoreChild->getOpCode().isConversion() || loadOrStoreChild->getOpCode().isLoad(), "Unexpected op");

               bool isUnsigned = loadOrStoreChild->getOpCode().isUnsigned();
               TR::InstOpCode::Mnemonic loadOp = getLoadOpCodeFromDataType(cg, dt, elementSize, isUnsigned, indexReg != NULL);

               TR::MemoryReference *arrayletMR = indexReg ?
                  new (cg->trHeapMemory()) TR::MemoryReference(arrayletReg, arrayletOffsetReg, cg) :
                  new (cg->trHeapMemory()) TR::MemoryReference(arrayletReg, arrayletOffsetVal, cg);
               generateTrg1MemInstruction(cg, loadOp, node, loadOrStoreReg, arrayletMR);

               if (doLoadDecompress)
                  {
                  TR_ASSERT(dt == TR::Address, "Expecting loads with decompression trees to have data type TR::Address");
                  genDecompressPointer(cg, node, loadOrStoreReg);
                  }
               }

            cg->stopUsingRegister(arrayletReg);
            }
         else
            {
            if (indexReg)
               generateTrg1Src2Instruction(cg, TR::InstOpCode::addx, node, loadOrStoreReg, loadOrStoreReg, arrayletOffsetReg);
            else
               addConstant32(cg, node, loadOrStoreReg, loadOrStoreReg, arrayletOffsetVal);
            }

         if (indexReg)
            srm->reclaimScratchRegister(arrayletOffsetReg);
         }

      const uint32_t numOOLDeps = 1 + (doLoadOrStore ? 1 : 0) + (needsBoundCheck && arrayLengthReg ? 1 : 0) + (loadOrStoreReg ? 1 : 0)
         + (indexReg ? 1 : 0) + srm->numAvailableRegisters();
      TR::RegisterDependencyConditions *OOLDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numOOLDeps, cg->trMemory());
      OOLDeps->addPostCondition(baseArrayReg, TR::RealRegister::NoReg);
      TR_ASSERT(OOLDeps->getPostConditions()->getRegisterDependency(0)->getRegister() == baseArrayReg, "Unexpected register");
      if (doLoadOrStore)
         {
         OOLDeps->addPostCondition(arrayletReg, TR::RealRegister::NoReg);
         TR_ASSERT(OOLDeps->getPostConditions()->getRegisterDependency(1)->getRegister() == arrayletReg, "Unexpected register");
         }
      if (indexReg)
         OOLDeps->addPostCondition(indexReg, TR::RealRegister::NoReg);
      if (loadOrStoreReg)
         OOLDeps->addPostCondition(loadOrStoreReg, TR::RealRegister::NoReg);
      if (needsBoundCheck && arrayLengthReg)
         OOLDeps->addPostCondition(arrayLengthReg, TR::RealRegister::NoReg);
      srm->addScratchRegistersToDependencyList(OOLDeps);

      srm->stopUsingRegisters();

      TR::LabelSymbol *doneOOLLabel = generateLabelSymbol(cg);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, doneOOLLabel, OOLDeps);
      generateLabelInstruction(cg, TR::InstOpCode::b, node, doneLabel);
      }
   discontiguousArrayOOL->swapInstructionListsWithCompilation();
   //
   // end of OOL

   cg->decReferenceCount(loadOrStoreChild);
   cg->decReferenceCount(baseArrayChild);
   cg->decReferenceCount(indexChild);

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
