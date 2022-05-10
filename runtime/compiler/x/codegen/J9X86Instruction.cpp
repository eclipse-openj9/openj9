/*******************************************************************************
 * Copyright (c) 2000, 2022 IBM Corp. and others
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

#include <algorithm>
#include "codegen/J9X86Instruction.hpp"
#include "env/OMRMemory.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/X86Instruction.hpp"
#include "codegen/Snippet.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "codegen/InstOpCode.hpp"

////////////////////////////////////////////////////////////////////////////////
// TR::X86MemImmSnippetInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86MemImmSnippetInstruction::X86MemImmSnippetInstruction(TR::InstOpCode::Mnemonic                op,
                                                             TR::Node                     *node,
                                                             TR::MemoryReference       *mr,
                                                             int32_t                      imm,
                                                             TR::UnresolvedDataSnippet *us,
                                                             TR::CodeGenerator            *cg)
      : TR::X86MemImmInstruction(imm, mr, node, op, cg), _unresolvedSnippet(us)
   {
   }

TR::X86MemImmSnippetInstruction::X86MemImmSnippetInstruction(TR::Instruction              *precedingInstruction,
                                                             TR::InstOpCode::Mnemonic                op,
                                                             TR::MemoryReference       *mr,
                                                             int32_t                      imm,
                                                             TR::UnresolvedDataSnippet *us,
                                                             TR::CodeGenerator            *cg)
      : TR::X86MemImmInstruction(imm, mr, op, precedingInstruction, cg), _unresolvedSnippet(us)
   {
   }

TR::Snippet *TR::X86MemImmSnippetInstruction::getSnippetForGC()
   {
   if (getUnresolvedSnippet() != NULL)
      {
      return getUnresolvedSnippet();
      }

   return getMemoryReference()->getUnresolvedDataSnippet();
   }

void TR::X86MemImmSnippetInstruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
   {
   TR::X86MemImmInstruction::assignRegisters(kindsToBeAssigned);
   if (kindsToBeAssigned & (TR_X87_Mask | TR_FPR_Mask))
      {
      TR::UnresolvedDataSnippet *snippet = getMemoryReference()->getUnresolvedDataSnippet();
      if (snippet)
         {
         if (kindsToBeAssigned & TR_X87_Mask)
            snippet->setNumLiveX87Registers(cg()->machine()->fpGetNumberOfLiveFPRs());

         if (kindsToBeAssigned & TR_FPR_Mask)
            snippet->setHasLiveXMMRegisters((cg()->machine()->fpGetNumberOfLiveXMMRs() > 0) ? true : false);
         }
      }
   }

uint8_t *TR::X86MemImmSnippetInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;

   setBinaryEncoding(instructionStart);

   cursor = getOpCode().binary(cursor, OMR::X86::Default, rexBits());
   cursor = getMemoryReference()->generateBinaryEncoding(cursor - 1, this, cg());
   if (cursor)
      {
      if (getOpCode().hasIntImmediate())
         {
         if (std::find(cg()->comp()->getStaticHCRPICSites()->begin(), cg()->comp()->getStaticHCRPICSites()->end(), this) != cg()->comp()->getStaticHCRPICSites()->end())
            {
            cg()->jitAdd32BitPicToPatchOnClassRedefinition(((void *)(uintptr_t)getSourceImmediateAsAddress()), (void *) cursor);
            }
         *(int32_t *)cursor = (int32_t)getSourceImmediate();
         if (getUnresolvedSnippet() != NULL)
            {
            getUnresolvedSnippet()->setAddressOfDataReference(cursor);
            }
         cursor += 4;
         }
      else if (getOpCode().hasByteImmediate() || getOpCode().hasSignExtendImmediate())
         {
         *(int8_t *)cursor = (int8_t)getSourceImmediate();
         cursor += 1;
         }
      else
         {
         *(int16_t *)cursor = (int16_t)getSourceImmediate();
         cursor += 2;
         }
      setBinaryLength(cursor - getBinaryEncoding());
      cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
      return cursor;
      }
   else
      {
      // Memref changed during binary encoding; re-generate current instruction.
      return generateBinaryEncoding();
      }
   }



TR::X86CheckAsyncMessagesMemImmInstruction::X86CheckAsyncMessagesMemImmInstruction(
   TR::Node               *node,
   TR::InstOpCode::Mnemonic          op,
   TR::MemoryReference *mr,
   int32_t                value,
   TR::CodeGenerator      *cg) :
   TR::X86MemImmInstruction(value, mr, node, op, cg)
   {
   }

TR::X86CheckAsyncMessagesMemRegInstruction::X86CheckAsyncMessagesMemRegInstruction(
   TR::Node               *node,
   TR::InstOpCode::Mnemonic          op,
   TR::MemoryReference *mr,
   TR::Register           *valueReg,
   TR::CodeGenerator      *cg) :
   TR::X86MemRegInstruction(valueReg, mr, node, op, cg)
   {
   }


TR::X86StackOverflowCheckInstruction::X86StackOverflowCheckInstruction(
   TR::Instruction        *precedingInstruction,
   TR::InstOpCode::Mnemonic          op,
   TR::Register           *cmpRegister,
   TR::MemoryReference *mr,
   TR::CodeGenerator      *cg) :
   TR::X86RegMemInstruction(mr, cmpRegister, op, precedingInstruction, cg)
   {
   }

uint8_t *TR::X86StackOverflowCheckInstruction::generateBinaryEncoding()
   {
   uint8_t *cursor = TR::X86RegMemInstruction::generateBinaryEncoding();
   return cursor;
   };




TR::X86StackOverflowCheckInstruction *generateStackOverflowCheckInstruction(
   TR::Instruction        *precedingInstruction,
   TR::InstOpCode::Mnemonic          op,
   TR::Register           *cmpRegister,
   TR::MemoryReference *mr,
   TR::CodeGenerator      *cg)
   {
   return new (cg->trHeapMemory()) TR::X86StackOverflowCheckInstruction(precedingInstruction, op, cmpRegister, mr, cg);
   }

TR::X86CheckAsyncMessagesMemImmInstruction *generateCheckAsyncMessagesInstruction(
   TR::Node               *node,
   TR::InstOpCode::Mnemonic          op,
   TR::MemoryReference *mr,
   int32_t                value,
   TR::CodeGenerator      *cg)
   {
   return new (cg->trHeapMemory()) TR::X86CheckAsyncMessagesMemImmInstruction(node, op, mr, value, cg);
   }

TR::X86CheckAsyncMessagesMemRegInstruction *generateCheckAsyncMessagesInstruction(
   TR::Node               *node,
   TR::InstOpCode::Mnemonic          op,
   TR::MemoryReference *mr,
   TR::Register           *reg,
   TR::CodeGenerator      *cg)
   {
   return new (cg->trHeapMemory()) TR::X86CheckAsyncMessagesMemRegInstruction(node, op, mr, reg, cg);
   }

uint8_t *TR::X86CheckAsyncMessagesMemImmInstruction::generateBinaryEncoding()
   {
   uint8_t *cursor = TR::X86MemImmInstruction::generateBinaryEncoding();
   return cursor;
   };

uint8_t *TR::X86CheckAsyncMessagesMemRegInstruction::generateBinaryEncoding()
   {
   uint8_t *cursor = TR::X86MemRegInstruction::generateBinaryEncoding();
   return cursor;
   };

TR::X86MemImmSnippetInstruction  *
generateMemImmSnippetInstruction(TR::InstOpCode::Mnemonic op, TR::Node * node, TR::MemoryReference  * mr, int32_t imm, TR::UnresolvedDataSnippet * snippet, TR::CodeGenerator *cg)
   {
   return new (cg->trHeapMemory()) TR::X86MemImmSnippetInstruction(op, node, mr, imm, snippet, cg);
   }

