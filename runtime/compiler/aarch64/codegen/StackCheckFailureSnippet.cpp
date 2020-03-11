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

#include "codegen/StackCheckFailureSnippet.hpp"

#include "codegen/ARM64Instruction.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Machine.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"

uint8_t *
TR::ARM64StackCheckFailureSnippet::emitSnippetBody()
   {
   TR::ResolvedMethodSymbol *bodySymbol = cg()->comp()->getJittedMethodSymbol();
   TR::Machine *machine = cg()->machine();
   TR::SymbolReference *sofRef = cg()->comp()->getSymRefTab()->findOrCreateStackOverflowSymbolRef(bodySymbol);
   ListIterator<TR::ParameterSymbol> paramIterator(&(bodySymbol->getParameterList()));
   TR::ParameterSymbol  *paramCursor = paramIterator.getFirst();

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
      uint32_t  numberOfParmSlots = atlas->getNumberOfParmSlotsMapped();
      TR_GCStackMap *map = new (cg()->trHeapMemory(), numberOfParmSlots) TR_GCStackMap(numberOfParmSlots);

      map->copy(atlas->getParameterMap());
      while (paramCursor != NULL)
         {
         int32_t  intRegArgIndex = paramCursor->getLinkageRegisterIndex();
         if (intRegArgIndex >= 0                   &&
             paramCursor->isReferencedParameter()  &&
             paramCursor->isCollectedReference())
            {
            // In full speed debug all the parameters are passed in the stack for this case
            // but will also reside in the registers
            if (!cg()->comp()->getOption(TR_FullSpeedDebug))
               {
               map->resetBit(paramCursor->getGCMapIndex());
               }
            map->setRegisterBits(1 << (linkage.getIntegerArgumentRegister(intRegArgIndex) - 1));
            }
         paramCursor = paramIterator.getNext();
         }

      // set the GC map
      gcMap().setStackMap(map);
      atlas->setParameterMap(map);
      }
   gcMap().registerStackMap(returnLocation, cg());

   return cursor+ARM64_INSTRUCTION_LENGTH;
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARM64StackCheckFailureSnippet * snippet)
   {
   TR::ResolvedMethodSymbol *bodySymbol = _comp->getJittedMethodSymbol();
   TR::SymbolReference *sofRef = _comp->getSymRefTab()->findOrCreateStackOverflowSymbolRef(bodySymbol);
   uint8_t *bufferPos  = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet));

   TR::Machine *machine = _cg->machine();
   TR::RealRegister *x9 = machine->getRealRegister(TR::RealRegister::x9);
   TR::RealRegister *stackPtr = _cg->getStackPointerRegister();
   TR::RealRegister *lr = machine->getRealRegister(TR::RealRegister::x30);
   uint32_t frameSize = _cg->getFrameSizeInBytes();

   if (frameSize <= 0xffff)
      {
      // mov x9, #frameSize
      printPrefix(pOutFile, NULL, bufferPos, 4);
      trfprintf(pOutFile, "movzx \t");
      print(pOutFile, x9, TR_WordReg);
      trfprintf(pOutFile, ", 0x%04x", frameSize);
      bufferPos += ARM64_INSTRUCTION_LENGTH;
      }
   else
      {
      TR_ASSERT(false, "Frame size too big.  Not supported yet");
      }

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "addx \t");
   print(pOutFile, stackPtr, TR_WordReg); trfprintf(pOutFile, ", ");
   print(pOutFile, stackPtr, TR_WordReg); trfprintf(pOutFile, ", ");
   print(pOutFile, x9, TR_WordReg);
   bufferPos += ARM64_INSTRUCTION_LENGTH;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "strimmx \t");
   print(pOutFile, lr, TR_WordReg);
   trfprintf(pOutFile, ", [");
   print(pOutFile, stackPtr, TR_WordReg);
   trfprintf(pOutFile, "]");
   bufferPos += ARM64_INSTRUCTION_LENGTH;

   char *info = "";
   intptr_t target = (intptr_t)(sofRef->getMethodAddress());
   int32_t distance;
   if (isBranchToTrampoline(sofRef, bufferPos, distance))
      {
      target = (intptr_t)distance + (intptr_t)bufferPos;
      info = " Through trampoline";
      }
   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "bl \t" POINTER_PRINTF_FORMAT "\t\t;%s", target, info);
   bufferPos += ARM64_INSTRUCTION_LENGTH;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "ldrimmx \t");
   print(pOutFile, lr, TR_WordReg);
   trfprintf(pOutFile, ", [");
   print(pOutFile, stackPtr, TR_WordReg);
   trfprintf(pOutFile, "]");
   bufferPos += ARM64_INSTRUCTION_LENGTH;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "subx \t");
   print(pOutFile, stackPtr, TR_WordReg); trfprintf(pOutFile, ", ");
   print(pOutFile, stackPtr, TR_WordReg); trfprintf(pOutFile, ", ");
   print(pOutFile, x9, TR_WordReg);
   bufferPos += ARM64_INSTRUCTION_LENGTH;

   intptr_t destination = (intptr_t)snippet->getReStartLabel()->getCodeLocation();
   // assuming that the distance to the destination is in the range +/- 128MB
   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t; Back to ", destination);
   print(pOutFile, snippet->getReStartLabel());
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
      return 0;
      }
   }
