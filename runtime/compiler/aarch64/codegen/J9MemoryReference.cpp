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
#include "codegen/CodeGenerator.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"

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
         TR::RealRegister *x9reg = cg->machine()->getRealRegister(TR::RealRegister::x9);
         uint32_t *wcursor = (uint32_t *)cursor;
         uint32_t preserve = *wcursor; // original instruction
         snippet->setAddressOfDataReference(cursor);
         snippet->setMemoryReference(self());
         cg->addRelocation(new (cg->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, snippet->getSnippetLabel()));
         *wcursor = 0x14000000; // b snippetLabel
         wcursor++;
         cursor += ARM64_INSTRUCTION_LENGTH;

         // insert instructions for computing the address of the resolved field;
         // the actual immediate operands are patched in by L_mergedDataResolve
         // in PicBuilder.spp

         // movk x9, #0, LSL #16
         *wcursor = 0xF2800000;
         x9reg->setRegisterFieldRD(wcursor);
         wcursor++;
         cursor += ARM64_INSTRUCTION_LENGTH;

         // movk x9, #0, LSL #32
         *wcursor = 0xF2C00000;
         x9reg->setRegisterFieldRD(wcursor);
         wcursor++;
         cursor += ARM64_INSTRUCTION_LENGTH;

         // movk x9, #0, LSL #48
         *wcursor = 0xF2E00000;
         x9reg->setRegisterFieldRD(wcursor);
         wcursor++;
         cursor += ARM64_INSTRUCTION_LENGTH;

         // finally, encode the original instruction
         *wcursor = preserve;
         TR::RealRegister *base = self()->getBaseRegister() ? toRealRegister(self()->getBaseRegister()) : NULL;
         if (base)
            {
            // if the load or store had a base, add it in as an index.
            base->setRegisterFieldRN(wcursor);
            x9reg->setRegisterFieldRM(wcursor);
            }
         else
            {
            x9reg->setRegisterFieldRN(wcursor);
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
