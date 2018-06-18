/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#include "codegen/Snippet.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/InstOpCode.hpp"
#include "infra/Assert.hpp"

/**
 * Helper method to insert Runtime Instrumentation Hooks RION or RIOFF in snippet
 *
 * @param cg               Code Generator Pointer
 * @param cursor           Current binary encoding cursor
 * @param op               Runtime Instrumentation opcode: TR::InstOpCode::RION or TR::InstOpCode::RIOFF
 * @param isPrivateLinkage Whether the snippet is involved in a private JIT linkage (i.e. Call Helper to JITCODE)
 * @return                 Updated binary encoding cursor after RI hook generation.
 */
uint8_t *
J9::Z::Snippet::generateRuntimeInstrumentationOnOffInstruction(TR::CodeGenerator *cg, uint8_t *cursor, TR::InstOpCode::Mnemonic op, bool isPrivateLinkage)
   {
   if (cg->getSupportsRuntimeInstrumentation())
      {
      if (!isPrivateLinkage || cg->getEnableRIOverPrivateLinkage())
         {
         if (op == TR::InstOpCode::RION)
            *(int32_t *) cursor = 0xAA010000;
         else if (op == TR::InstOpCode::RIOFF)
            *(int32_t *) cursor = 0xAA030000;
         else
            TR_ASSERT( 0, "Unexpected RI opcode.");
         cursor += sizeof(int32_t);
         }
      }
   return cursor;
   }


/**
 * Helper method to query the length of Runtime Instrumentation Hooks RION or RIOFF in snippet
 *
 * @param cg               Code Generator Pointer
 * @param isPrivateLinkage Whether the snippet is involved in a private JIT linkage (i.e. Call Helper to JITCODE)
 * @return                 The length of RION or RIOFF encoding if generated.  Zero otherwise.
 */
uint32_t
J9::Z::Snippet::getRuntimeInstrumentationOnOffInstructionLength(TR::CodeGenerator *cg, bool isPrivateLinkage)
   {
   if (cg->getSupportsRuntimeInstrumentation())
      {
      if (!isPrivateLinkage || cg->getEnableRIOverPrivateLinkage())
         return sizeof(int32_t);  // Both RION and RIOFF are 32-bit (4-byte) instructions.
      }
   return 0;
   }
