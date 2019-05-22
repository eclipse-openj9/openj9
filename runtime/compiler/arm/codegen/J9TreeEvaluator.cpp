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

#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "omrmodroncore.h"
#include "thrdsup.h"
#include "thrtypes.h"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "arm/codegen/ARMInstruction.hpp"
#include "arm/codegen/GenerateInstructions.hpp"

/**
 *  \brief
 *     Generates code for card marking checks
 *
 *  \parm dstReg
 *     Register for destination object
 *
 *  \parm temp1Reg
 *     Temporary register
 *
 *  \parm temp2Reg
 *     Temporary register
 *
 *  \parm temp3Reg
 *     Temporary register
 *
 *  \parm deps
 *     Register dependencies to be satisfied at the end of code
 */
static void VMCardCheckEvaluator(TR::Node *node, TR::Register *dstReg, TR::Register *temp1Reg, TR::Register *temp2Reg, TR::Register *temp3Reg,
      TR::RegisterDependencyConditions *deps, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   // non-heap objects cannot be marked
   // Make sure we really should be here
   TR::Options *options = comp->getOptions();
   TR::Node *wrtbarNode = NULL;
   bool definitelyHeapObject = false, definitelyNonHeapObject = false;
   auto gcMode = TR::Compiler->om.writeBarrierType();

   if (node->getOpCodeValue() == TR::awrtbari || node->getOpCodeValue() == TR::awrtbar)
      wrtbarNode = node;
   else if (node->getOpCodeValue() == TR::ArrayStoreCHK)
      wrtbarNode = node->getFirstChild();

   if (wrtbarNode != NULL)
      {
      definitelyHeapObject = wrtbarNode->isHeapObjectWrtBar();
      definitelyNonHeapObject = wrtbarNode->isNonHeapObjectWrtBar();
      }

   TR_ASSERT((gcMode == gc_modron_wrtbar_cardmark || gcMode == gc_modron_wrtbar_cardmark_and_oldcheck || gcMode == gc_modron_wrtbar_cardmark_incremental) && !definitelyNonHeapObject,
         "VMCardCheckEvaluator: Invalid call to cardCheckEvaluator\n");

   if (!definitelyNonHeapObject)
      {
      TR::Register *metaReg = cg->getMethodMetaDataRegister();
      TR::LabelSymbol *noChkLabel = generateLabelSymbol(cg);

      // Balanced policy must always dirty the card table.
      //
      if (gcMode != gc_modron_wrtbar_cardmark_incremental)
         {
         uint32_t base, rotate;

         generateTrg1MemInstruction(cg, ARMOp_ldr, node, temp1Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, privateFlags), cg));

         // The value for J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE is a generated value when VM code is created
         // At the moment we are safe here, but it is better to be careful and avoid any unexpected behaviour
         // Make sure this falls within the scope of tst
         //
         TR_ASSERT(J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE >= 0x00010000 && J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE <= 0x80000000,
               "Concurrent mark active Value assumption broken.");
         constantIsImmed8r(J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE, &base, &rotate);
         generateSrc1ImmInstruction(cg, ARMOp_tst, node, temp1Reg, base, rotate);
         generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, noChkLabel);
         }

      uintptr_t card_size_shift = trailingZeroes((uint32_t) options->getGcCardSize());

      // temp3Reg = dstReg - heapBaseForBarrierRange0
      generateTrg1MemInstruction(cg, ARMOp_ldr, node, temp3Reg,
            new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapBaseForBarrierRange0), cg));
      generateTrg1Src2Instruction(cg, ARMOp_sub, node, temp3Reg, dstReg, temp3Reg);

      if (!definitelyHeapObject)
         {
         // if (temp3Reg >= heapSizeForBarrierRange0), object not in the heap
         generateTrg1MemInstruction(cg, ARMOp_ldr, node, temp1Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapSizeForBarrierRange0), cg));
         generateSrc2Instruction(cg, ARMOp_cmp, node, temp3Reg, temp1Reg);
         generateConditionalBranchInstruction(cg, node, ARMConditionCodeCS, noChkLabel);
         }

      // dirty(activeCardTableBase + temp3Reg >> card_size_shift)
      generateTrg1MemInstruction(cg, ARMOp_ldr, node, temp1Reg,
            new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, activeCardTableBase), cg));
      generateShiftRightImmediate(cg, node, temp3Reg, temp3Reg, card_size_shift, true);
      armLoadConstant(node, CARD_DIRTY, temp2Reg, cg);
      generateMemSrc1Instruction(cg, ARMOp_strb, node, new (cg->trHeapMemory()) TR::MemoryReference(temp1Reg, temp3Reg, 1, cg), temp2Reg);

      generateLabelInstruction(cg, ARMOp_label, node, noChkLabel, deps);
      }

   }


void
J9::ARM::TreeEvaluator::generateTestAndReportFieldWatchInstructions(TR::CodeGenerator *cg, TR::Node *node, TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister, TR::Register *valueReg)
   {
   TR_ASSERT_FATAL(false, "This helper implements platform specific code for Fieldwatch, which is currently not supported on ARM platforms.\n");
   }

void
J9::ARM::TreeEvaluator::generateFillInDataBlockSequenceForUnresolvedField(TR::CodeGenerator *cg, TR::Node *node, TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister)
   {
   TR_ASSERT_FATAL(false, "This helper implements platform specific code for Fieldwatch, which is currently not supported on ARM platforms.\n");
   }

/**
 *  \brief
 *     Generates write barrier for non-simple arraycopy node
 *
 *  \parm srcObjReg
 *     Register for source object
 *
 *  \parm dstObjReg
 *     Register for destination object
 */
void J9::ARM::TreeEvaluator::genWrtbarForArrayCopy(TR::Node *node, TR::Register *srcObjReg, TR::Register *dstObjReg, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   bool ageCheckIsNeeded = false;
   bool cardMarkIsNeeded = false;
   auto gcMode = TR::Compiler->om.writeBarrierType();

   ageCheckIsNeeded = (gcMode == gc_modron_wrtbar_oldcheck || gcMode == gc_modron_wrtbar_cardmark_and_oldcheck || gcMode == gc_modron_wrtbar_always);
   cardMarkIsNeeded = (gcMode == gc_modron_wrtbar_cardmark || gcMode == gc_modron_wrtbar_cardmark_incremental);

   if (!ageCheckIsNeeded && !cardMarkIsNeeded)
      {
      return;
      }

   if (ageCheckIsNeeded)
      {
      TR::Register *temp1Reg;
      TR::Register *temp2Reg;
      TR::RegisterDependencyConditions *conditions;

      TR::LabelSymbol *doneLabel;
      TR::SymbolReference *wbRef = comp->getSymRefTab()->findOrCreateWriteBarrierBatchStoreSymbolRef(comp->getMethodSymbol());

      if (gcMode != gc_modron_wrtbar_always)
         {
         temp1Reg = cg->allocateRegister();
         temp2Reg = cg->allocateRegister();
         doneLabel = generateLabelSymbol(cg);

         conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(3, 3, cg->trMemory());
         TR::addDependency(conditions, temp1Reg, TR::RealRegister::NoReg, TR_GPR, cg);
         TR::addDependency(conditions, temp2Reg, TR::RealRegister::NoReg, TR_GPR, cg);

         TR::Register *metaReg = cg->getMethodMetaDataRegister();

         // temp1Reg = dstObjReg - heapBaseForBarrierRange0
         generateTrg1MemInstruction(cg, ARMOp_ldr, node, temp1Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapBaseForBarrierRange0), cg));
         generateTrg1Src2Instruction(cg, ARMOp_sub, node, temp1Reg, dstObjReg, temp1Reg);

         // if (temp1Reg >= heapSizeForBarrierRange0), object not in the tenured area
         generateTrg1MemInstruction(cg, ARMOp_ldr, node, temp2Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapSizeForBarrierRange0), cg));
         generateSrc2Instruction(cg, ARMOp_cmp, node, temp1Reg, temp2Reg);
         generateConditionalBranchInstruction(cg, node, ARMConditionCodeCS, doneLabel);
         }
      else
         {
         conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());
         }

      TR::addDependency(conditions, dstObjReg, TR::RealRegister::gr0, TR_GPR, cg);

      TR::Instruction *gcPoint = generateImmSymInstruction(cg, ARMOp_bl, node, (uintptr_t)wbRef->getSymbol()->castToMethodSymbol()->getMethodAddress(), NULL, wbRef);
      gcPoint->ARMNeedsGCMap(0xFFFFFFFF);

      if (gcMode != gc_modron_wrtbar_always)
         {
         generateLabelInstruction(cg, ARMOp_label, node, doneLabel, conditions);
         }

      cg->machine()->setLinkRegisterKilled(true);
      cg->setHasCall();

      if (gcMode != gc_modron_wrtbar_always)
         {
         cg->stopUsingRegister(temp1Reg);
         cg->stopUsingRegister(temp2Reg);
         }
      }
   else if (cardMarkIsNeeded)
      {
      if (!TR::Options::getCmdLineOptions()->realTimeGC())
         {
         TR::Register *temp1Reg = cg->allocateRegister();
         TR::Register *temp2Reg = cg->allocateRegister();
         TR::Register *temp3Reg = cg->allocateRegister();
         TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(4, 4, cg->trMemory());

         TR::addDependency(conditions, dstObjReg, TR::RealRegister::NoReg, TR_GPR, cg);
         TR::addDependency(conditions, temp1Reg, TR::RealRegister::NoReg, TR_GPR, cg);
         TR::addDependency(conditions, temp2Reg, TR::RealRegister::NoReg, TR_GPR, cg);
         TR::addDependency(conditions, temp3Reg, TR::RealRegister::NoReg, TR_GPR, cg);

         VMCardCheckEvaluator(node, dstObjReg, temp1Reg, temp2Reg, temp3Reg, conditions, cg);

         cg->stopUsingRegister(temp1Reg);
         cg->stopUsingRegister(temp2Reg);
         cg->stopUsingRegister(temp3Reg);
         }
      else
         TR_ASSERT(0, "genWrtbarForArrayCopy card marking not supported for RT");
      }
   }
