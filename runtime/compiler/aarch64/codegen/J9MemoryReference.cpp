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
#include "codegen/CodeGenerator.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"

J9::ARM64::MemoryReference::MemoryReference(
      TR::Node *node,
      TR::CodeGenerator *cg)
   : OMR::MemoryReferenceConnector(node, cg)
   {
   if (self()->getUnresolvedSnippet())
      self()->adjustForResolution(cg);
   }

J9::ARM64::MemoryReference::MemoryReference(
      TR::Node *node,
      TR::SymbolReference *symRef,
      TR::CodeGenerator *cg)
   : OMR::MemoryReferenceConnector(node, symRef, cg)
   {
   if (self()->getUnresolvedSnippet())
      self()->adjustForResolution(cg);
   }

void
J9::ARM64::MemoryReference::adjustForResolution(TR::CodeGenerator *cg)
   {
   self()->setExtraRegister(cg->allocateRegister());
   }

void
J9::ARM64::MemoryReference::assignRegisters(TR::Instruction *currentInstruction, TR::CodeGenerator *cg)
   {
   TR::Machine *machine = cg->machine();
   TR::Register *baseRegister = self()->getBaseRegister();
   TR::Register *indexRegister = self()->getIndexRegister();
   TR::Register *extraRegister = self()->getExtraRegister();
   TR::RealRegister *assignedBaseRegister;
   TR::RealRegister *assignedIndexRegister;
   TR::RealRegister *assignedExtraRegister;

   if (baseRegister != NULL)
      {
      assignedBaseRegister = baseRegister->getAssignedRealRegister();
      if (indexRegister != NULL)
         {
         indexRegister->block();
         }
      if (extraRegister != NULL)
         {
         extraRegister->block();
         }

      if (assignedBaseRegister == NULL)
         {
         if (baseRegister->getTotalUseCount() == baseRegister->getFutureUseCount())
            {
            if ((assignedBaseRegister = machine->findBestFreeRegister(currentInstruction, TR_GPR, true, baseRegister)) == NULL)
               {
               assignedBaseRegister = machine->freeBestRegister(currentInstruction, baseRegister);
               }
            }
         else
            {
            assignedBaseRegister = machine->reverseSpillState(currentInstruction, baseRegister);
            }
         baseRegister->setAssignedRegister(assignedBaseRegister);
         assignedBaseRegister->setAssignedRegister(baseRegister);
         assignedBaseRegister->setState(TR::RealRegister::Assigned);
         }

      if (indexRegister != NULL)
         {
         indexRegister->unblock();
         }
      if (extraRegister != NULL)
         {
         extraRegister->unblock();
         }
      }

   if (indexRegister != NULL)
      {
      if (baseRegister != NULL)
         {
         baseRegister->block();
         }
      if (extraRegister != NULL)
         {
         extraRegister->block();
         }

      assignedIndexRegister = indexRegister->getAssignedRealRegister();
      if (assignedIndexRegister == NULL)
         {
         if (indexRegister->getTotalUseCount() == indexRegister->getFutureUseCount())
            {
            if ((assignedIndexRegister = machine->findBestFreeRegister(currentInstruction, TR_GPR, false, indexRegister)) == NULL)
               {
               assignedIndexRegister = machine->freeBestRegister(currentInstruction, indexRegister);
               }
            }
         else
            {
            assignedIndexRegister = machine->reverseSpillState(currentInstruction, indexRegister);
            }
         indexRegister->setAssignedRegister(assignedIndexRegister);
         assignedIndexRegister->setAssignedRegister(indexRegister);
         assignedIndexRegister->setState(TR::RealRegister::Assigned);
         }

      if (baseRegister != NULL)
         {
         baseRegister->unblock();
         }
      if (extraRegister != NULL)
         {
         extraRegister->unblock();
         }
      }

   if (extraRegister != NULL)
      {
      if (baseRegister != NULL)
         {
         baseRegister->block();
         }
      if (indexRegister != NULL)
         {
         indexRegister->block();
         }

      assignedExtraRegister = extraRegister->getAssignedRealRegister();
      if (assignedExtraRegister == NULL)
         {
         if (extraRegister->getTotalUseCount() == extraRegister->getFutureUseCount())
            {
            if ((assignedExtraRegister = machine->findBestFreeRegister(currentInstruction, TR_GPR, false, extraRegister)) == NULL)
               {
               assignedExtraRegister = machine->freeBestRegister(currentInstruction, extraRegister);
               }
            }
         else
            {
            assignedExtraRegister = machine->reverseSpillState(currentInstruction, extraRegister);
            }
         extraRegister->setAssignedRegister(assignedExtraRegister);
         assignedExtraRegister->setAssignedRegister(extraRegister);
         assignedExtraRegister->setState(TR::RealRegister::Assigned);
         }

      if (baseRegister != NULL)
         {
         baseRegister->unblock();
         }
      if (indexRegister != NULL)
         {
         indexRegister->unblock();
         }
      }

   if (baseRegister != NULL)
      {
      machine->decFutureUseCountAndUnlatch(currentInstruction, baseRegister);
      self()->setBaseRegister(assignedBaseRegister);
      }

   if (indexRegister != NULL)
      {
      machine->decFutureUseCountAndUnlatch(currentInstruction, indexRegister);
      self()->setIndexRegister(assignedIndexRegister);
      }

   if (extraRegister != NULL)
      {
      machine->decFutureUseCountAndUnlatch(currentInstruction, extraRegister);
      TR_ASSERT(extraRegister->getFutureUseCount() == 0, "Unexpected future use count for extraReg");
      self()->setExtraRegister(assignedExtraRegister);
      }

   if (self()->getUnresolvedSnippet() != NULL)
      {
      currentInstruction->ARM64NeedsGCMap(cg, 0xFFFFFFFF);
      }
   }

uint8_t *
J9::ARM64::MemoryReference::generateBinaryEncoding(TR::Instruction *currentInstruction, uint8_t *cursor, TR::CodeGenerator *cg)
   {
   TR::UnresolvedDataSnippet *snippet = self()->getUnresolvedSnippet();

   if (snippet)
      {
      if (self()->getIndexRegister())
         {
         TR_ASSERT(false, "Unresolved indexed snippet is not supported");
         return NULL;
         }
      else
         {
         uint32_t *wcursor = (uint32_t *)cursor;
         uint32_t preserve = *wcursor; // original instruction
         snippet->setAddressOfDataReference(cursor);
         snippet->setMemoryReference(self());
         cg->addRelocation(new (cg->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, snippet->getSnippetLabel()));
         *wcursor = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::b); // b snippetLabel
         wcursor++;
         cursor += ARM64_INSTRUCTION_LENGTH;

         TR_ASSERT(self()->getExtraRegister(), "extraReg must have been allocated");
         TR::RealRegister *extraReg = toRealRegister(self()->getExtraRegister());

         // insert instructions for computing the address of the resolved field;
         // the actual immediate operands are patched in by L_mergedDataResolve
         // in PicBuilder.spp

         // movk extraReg, #0, LSL #16
         *wcursor = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::movkx) | (TR::MOV_LSL16 << 5);
         extraReg->setRegisterFieldRD(wcursor);
         wcursor++;
         cursor += ARM64_INSTRUCTION_LENGTH;

         // movk extraReg, #0, LSL #32
         *wcursor = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::movkx) | (TR::MOV_LSL32 << 5);
         extraReg->setRegisterFieldRD(wcursor);
         wcursor++;
         cursor += ARM64_INSTRUCTION_LENGTH;

         // movk extraReg, #0, LSL #48
         *wcursor = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::movkx) | (TR::MOV_LSL48 << 5);
         extraReg->setRegisterFieldRD(wcursor);
         wcursor++;
         cursor += ARM64_INSTRUCTION_LENGTH;

         // finally, encode the original instruction
         *wcursor = preserve;
         TR::RealRegister *base = self()->getBaseRegister() ? toRealRegister(self()->getBaseRegister()) : NULL;
         TR::InstOpCode::Mnemonic op = currentInstruction->getOpCodeValue();
         if (op != TR::InstOpCode::addx)
            {
            // load/store instruction
            if (op == TR::InstOpCode::ldrimmx || op == TR::InstOpCode::strimmx
                || op == TR::InstOpCode::ldrimmw || op == TR::InstOpCode::strimmw
                || op == TR::InstOpCode::ldrhimm || op == TR::InstOpCode::strhimm
                || op == TR::InstOpCode::ldrshimmx || op == TR::InstOpCode::ldrshimmw
                || op == TR::InstOpCode::ldrbimm || op == TR::InstOpCode::strbimm
                || op == TR::InstOpCode::ldrsbimmx || op == TR::InstOpCode::ldrsbimmw
                || op == TR::InstOpCode::vldrimmd || op == TR::InstOpCode::vstrimmd
                || op == TR::InstOpCode::vldrimms || op == TR::InstOpCode::vstrimms)
               {
               if (base)
                  {
                  // if the load or store had a base, add it in as an index.
                  base->setRegisterFieldRN(wcursor);
                  extraReg->setRegisterFieldRM(wcursor);
                  // Rewrite the instruction from "ldr Rt, [Rn, #imm]" to "ldr Rt, [Rn, Rm]"
                  cursor[1] |= 0x68; // Set bits 11, 13, 14
                  cursor[2] |= 0x20; // Set bit 21
                  cursor[3] &= 0xFE; // Clear bit 24
                  }
               else
                  {
                  // ldr Rt, [Rn]
                  extraReg->setRegisterFieldRN(wcursor);
                  }
               }
            else
               {
               TR_ASSERT_FATAL(false, "Unsupported load/store instruction for unresolved data snippet");
               }
            }
         else
            {
            // loadaddrEvaluator() uses addx in generateTrgMemInstruction
            if (base)
               {
               // addx Rt, Rn, Rm
               base->setRegisterFieldRN(wcursor);
               extraReg->setRegisterFieldRM(wcursor);
               }
            else
               {
               extraReg->setRegisterFieldRN(wcursor);
               // Rewrite the instruction from "addx Rd, Rn, Rm" to "addimmx Rd, Rn, #0"
               cursor[3] = (uint8_t)0x91;
               }
            }
         cursor += ARM64_INSTRUCTION_LENGTH;

         return cursor;
         }
      }
   else
      {
      return OMR::MemoryReferenceConnector::generateBinaryEncoding(currentInstruction, cursor, cg);
      }
   }

uint32_t
J9::ARM64::MemoryReference::estimateBinaryLength(TR::InstOpCode op)
   {
   if (self()->getUnresolvedSnippet())
      {
      if (self()->getIndexRegister())
         {
         TR_ASSERT(false, "Unresolved indexed snippet is not supported");
         return 0;
         }
      else
         {
         return 5 * ARM64_INSTRUCTION_LENGTH;
         }
      }
   else
      {
      return OMR::MemoryReferenceConnector::estimateBinaryLength(op);
      }
   }
