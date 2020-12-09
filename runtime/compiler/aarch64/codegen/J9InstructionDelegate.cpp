/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "codegen/ARM64Instruction.hpp"
#include "codegen/CallSnippet.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/InstructionDelegate.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

void
J9::ARM64::InstructionDelegate::encodeBranchToLabel(TR::CodeGenerator *cg, TR::ARM64ImmSymInstruction *ins, uint8_t *cursor)
   {
   ((TR::ARM64CallSnippet *)ins->getCallSnippet())->setCallRA(cursor + ARM64_INSTRUCTION_LENGTH);
   }

static void
setupImplicitNullPointerExceptionImpl(TR::CodeGenerator *cg, TR::Instruction *instr, TR::Node *node, TR::MemoryReference *mr)
   {
   TR::Compilation *comp = cg->comp();
   if(cg->getHasResumableTrapHandler())
      {
      // If the treetop node is BNDCHK node combined with NULLCHK, the previous treetop node is NULLCHK.
      auto treeTopNode = cg->getCurrentEvaluationTreeTop()->getNode();
      if (treeTopNode->chkFoldedImplicitNULLCHK())
         {
         treeTopNode = cg->getCurrentEvaluationTreeTop()->getPrevTreeTop()->getNode();
         }
      // this instruction throws an implicit null check if:
      // 1. The treetop node is a NULLCHK node
      // 2. The memory reference of this instruction can cause a null pointer exception
      // 3. The null check reference node must be a child of this node
      // 4. This memory reference uses the same register as the null check reference
      // 5. This is the first instruction in the evaluation of this null check node to have met all the conditions

      // Test conditions 1, 2, and 5
      if(node != NULL & mr != NULL &&
         mr->getCausesImplicitNullPointerException() &&
         treeTopNode->getOpCode().isNullCheck() &&
         cg->getImplicitExceptionPoint() == NULL)
         {
         // determine what the NULLcheck reference node is
         TR::Node * nullCheckReference;
         TR::Node * firstChild = treeTopNode->getFirstChild();
         if (comp->useCompressedPointers() &&
               firstChild->getOpCodeValue() == TR::l2a)
            {
            TR::ILOpCodes loadOp = comp->il.opCodeForIndirectLoad(TR::Int32);
            TR::ILOpCodes rdbarOp = comp->il.opCodeForIndirectReadBarrier(TR::Int32);
            while (firstChild->getOpCodeValue() != loadOp && firstChild->getOpCodeValue() != rdbarOp)
               firstChild = firstChild->getFirstChild();
            nullCheckReference = firstChild->getFirstChild();
            }
         else
            nullCheckReference = treeTopNode->getNullCheckReference();

         TR::Register *nullCheckReg = nullCheckReference->getRegister();
         // Test conditions 3 and 4
         if ((node->getOpCode().hasSymbolReference() &&
               node->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef()) ||
             (node->hasChild(nullCheckReference) && (nullCheckReg != NULL) && mr->refsRegister(nullCheckReg)))
            {
            traceMsg(comp,"Instruction %p throws an implicit NPE, node: %p NPE node: %p\n", instr, node, nullCheckReference);
            cg->setImplicitExceptionPoint(instr);
            }
         }
      }
   }

void
J9::ARM64::InstructionDelegate::setupImplicitNullPointerException(TR::CodeGenerator *cg, TR::ARM64Trg1MemInstruction *instr)
   {
   setupImplicitNullPointerExceptionImpl(cg, instr, instr->getNode(), instr->getMemoryReference());
   }

void
J9::ARM64::InstructionDelegate::setupImplicitNullPointerException(TR::CodeGenerator *cg, TR::ARM64MemInstruction *instr)
   {
   setupImplicitNullPointerExceptionImpl(cg, instr, instr->getNode(), instr->getMemoryReference());
   }
