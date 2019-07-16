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

#include "codegen/StackCheckFailureSnippet.hpp"

#include "codegen/ARM64Instruction.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Machine.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"

uint8_t *
TR::ARM64StackCheckFailureSnippet::emitSnippetBody()
   {
   TR::ResolvedMethodSymbol *bodySymbol = cg()->comp()->getJittedMethodSymbol();
   TR::Machine *machine = cg()->machine();
   TR::SymbolReference *sofRef = cg()->comp()->getSymRefTab()->findOrCreateStackOverflowSymbolRef(bodySymbol);

   uint8_t *cursor = cg()->getBinaryBufferCursor();
   uint8_t *returnLocation;

   getSnippetLabel()->setCodeLocation(cursor);

   // Pass cg()->getFrameSizeInBytes() to jitStackOverflow in register x9.
   //
   const TR::ARM64LinkageProperties &linkage = cg()->getLinkage()->getProperties();
   uint32_t frameSize = cg()->getFrameSizeInBytes();

   if (frameSize <= 0xffff)
      {
      // mov x9, #frameSize
      *(int32_t *)cursor = 0xd2800009 | (frameSize << 5);
      cursor += ARM64_INSTRUCTION_LENGTH;
      }
   else
      {
      TR_ASSERT(false, "Frame size too big.  Not supported yet");
      }

   // add J9SP, J9SP, x9
   *(int32_t *)cursor = 0x8b090294;
   cursor += ARM64_INSTRUCTION_LENGTH;

   // str LR, [J9SP]
   // ToDo: Skip saving/restoring LR when not required
   *(int32_t *)cursor = 0xf900029e;
   cursor += ARM64_INSTRUCTION_LENGTH;

   // bl helper
   *(int32_t *)cursor = cg()->encodeHelperBranchAndLink(sofRef, cursor, getNode());
   cursor += ARM64_INSTRUCTION_LENGTH;
   returnLocation = cursor;

   // ldr LR, [J9SP]
   *(int32_t *)cursor = 0xf940029e;
   cursor += ARM64_INSTRUCTION_LENGTH;

   // sub J9SP, J9SP, x9 ; assume that jitStackOverflow does not clobber x9
   *(int32_t *)cursor = 0xcb090294;
   cursor += ARM64_INSTRUCTION_LENGTH;

   // b restartLabel
   intptr_t destination = (intptr_t)getReStartLabel()->getCodeLocation();
   if (!cg()->directCallRequiresTrampoline(destination, (intptr_t)cursor))
      {
      intptr_t distance = destination - (intptr_t)cursor;
      *(int32_t *)cursor = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::b) | ((distance >> 2) & 0x3ffffff); // imm26
      }
   else
      {
      TR_ASSERT(false, "Target too far away.  Not supported yet");
      }

   TR::GCStackAtlas *atlas = cg()->getStackAtlas();
   if (atlas)
      {
      // only the arg references are live at this point
      TR_GCStackMap *map = atlas->getParameterMap();

      // set the GC map
      gcMap().setStackMap(map);
      }
   gcMap().registerStackMap(returnLocation, cg());

   return cursor+ARM64_INSTRUCTION_LENGTH;
   }

uint32_t
TR::ARM64StackCheckFailureSnippet::getLength(int32_t estimatedSnippetStart)
   {
   uint32_t frameSize = cg()->getFrameSizeInBytes();

   if (frameSize <= 0xffff)
      {
      return 7*ARM64_INSTRUCTION_LENGTH;
      }
   else
      {
      TR_ASSERT(false, "Frame size too big.  Not supported yet");
      }
   }
