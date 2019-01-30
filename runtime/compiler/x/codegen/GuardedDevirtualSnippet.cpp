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

#include "x/codegen/GuardedDevirtualSnippet.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/SnippetGCMap.hpp"
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "ras/Debug.hpp"
#include "x/codegen/RestartSnippet.hpp"

namespace TR { class Block; }
namespace TR { class MethodSymbol; }
namespace TR { class Register; }

TR::X86GuardedDevirtualSnippet::X86GuardedDevirtualSnippet(TR::CodeGenerator * cg,
                                                               TR::Node          * node,
                                                               TR::LabelSymbol *restartlab,
                                                               TR::LabelSymbol *snippetlab,
                                                               int32_t         vftoffset,
                                                               TR::Block       *currentBlock,
                                                               TR::Register    *classRegister)
   : TR::X86RestartSnippet(cg, node, restartlab, snippetlab, true),
     _vtableOffset(vftoffset),
     _currentBlock(currentBlock),
     _classObjectRegister(classRegister)
   {
   }


TR::X86GuardedDevirtualSnippet  *TR::X86GuardedDevirtualSnippet::getGuardedDevirtualSnippet()
   {
   return this;
   }

uint8_t *TR::X86GuardedDevirtualSnippet::emitSnippetBody()
   {
   uint8_t *buffer = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(buffer);
   TR::Compilation *comp = cg()->comp();

   if (_classObjectRegister == NULL)
      {
      if (TR::Compiler->target.is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders())
         *buffer++ = 0x48; // Rex prefix for 64-bit mov

      *(uint16_t *)buffer = 0x788b; // prepare for mov edi, [eax + class_offset]
      buffer += 2;

      *(uint8_t *)buffer = (uint8_t) TR::Compiler->om.offsetOfObjectVftField(); // mov edi, [eax + class_offset]
      buffer += 1;

      uintptrj_t vftMask = TR::Compiler->om.maskOfObjectVftField();
      if (~vftMask != 0)
         {
         if (TR::Compiler->target.is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders())
            *buffer++ = 0x48; // Rex prefix

         // and edi, vftMask
         //
         *(uint16_t *)buffer = 0xe781;
         buffer += 2;
         *(uint32_t *)buffer = vftMask;
         buffer += 4;
         }

      // two opcode bytes for call [edi + Imm] instruction
      *(uint16_t *)buffer = 0x97ff;
      buffer += 2;
      }
   else
      {
      if (TR::Compiler->target.is64Bit())
         {
         uint8_t rex = toRealRegister(_classObjectRegister)->rexBits(TR::RealRegister::REX_B, false);
         if (rex)
            *buffer++ = rex;
         }

      // two opcode bytes for call [reg + Imm] instruction, fix the low 3 bits (later)
      // depending on what register the class object is in.  For now, leave it zero
      *(uint16_t *)buffer = 0x90ff;
      buffer++;

      // use SIB-byte encoding if necessary
      if (toRealRegister(_classObjectRegister)->needsSIB())
         {
         *buffer++ |= IA32SIBPresent;
         *buffer    = IA32SIBNoIndex;
         }

      // now must get the class register and add its encoding to the low order 3 bits
      // of the second opcode byte (0x90);
      TR::RealRegister *classObjectRegister = toRealRegister(_classObjectRegister);
      classObjectRegister->setRMRegisterFieldInModRM(buffer++);
      }


   // set the immediate offset field to be the vtable index
   *(int32_t *)buffer = _vtableOffset;       // call [reg + vtable offset]
   buffer += 4;

   gcMap().registerStackMap(buffer, cg());

   return genRestartJump(buffer);
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::X86GuardedDevirtualSnippet  * snippet)
   {
   if (pOutFile == NULL)
      return;

   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet), "out of line full virtual call sequence");

   char regLetter = (TR::Compiler->target.is64Bit())? 'r' : 'e';

#if defined(TR_TARGET_64BIT)
   TR::Node            *callNode     = snippet->getNode();
   TR::SymbolReference *methodSymRef = snippet->getRealMethodSymbolReference();
   if (!methodSymRef)
      methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   // Show effect of loadArgumentsIfNecessary on devirtualized VMInternalNatives.
   if (snippet->isLoadArgumentsNecessary(methodSymbol))
      bufferPos = printArgumentFlush(pOutFile, callNode, false, bufferPos);
#endif

   if (!snippet->getClassObjectRegister())
      {
      int movSize = (TR::Compiler->target.is64Bit())? 3 : 2;

      printPrefix(pOutFile, NULL, bufferPos, movSize);
      trfprintf(pOutFile, "mov \t%cdi, [%cax]\t\t%s Load Class Object",
                    regLetter, regLetter,
                    commentString());
      bufferPos += movSize;

      printPrefix(pOutFile, NULL, bufferPos, 6);
      trfprintf(pOutFile, "call\t[%cdi %d]\t\t%s call through vtable slot %d", regLetter, snippet->getVTableOffset(),
                               commentString(), -snippet->getVTableOffset() >> 2);
      bufferPos += 6;
      }
   else
      {
      TR::RealRegister *classObjectRegister = toRealRegister(snippet->getClassObjectRegister());
#if defined(TR_TARGET_64BIT)
      int callSize = 6;
      if (toRealRegister(classObjectRegister)->rexBits(TR::RealRegister::REX_B, false))
         callSize++;
      if (toRealRegister(classObjectRegister)->needsSIB())
         callSize++;
      TR_RegisterSizes regSize = TR_DoubleWordReg;
#else
      int callSize = 6;
      TR_RegisterSizes regSize = TR_WordReg;
#endif

      printPrefix(pOutFile, NULL, bufferPos, callSize);
      trfprintf(pOutFile,
              "call\t[%s %d]\t\t%s call through vtable slot %d",
              getName(classObjectRegister, regSize),
              snippet->getVTableOffset(),
              commentString(),
              -snippet->getVTableOffset() >> 2);
      bufferPos += callSize;
      }

   printRestartJump(pOutFile, snippet, bufferPos);
   }


uint32_t TR::X86GuardedDevirtualSnippet::getLength(int32_t estimatedSnippetStart)
   {
   uint32_t fixedLength;
   if (_classObjectRegister)
      {
      fixedLength = 6 + (toRealRegister(_classObjectRegister)->needsSIB()? 1 : 0);
      if (TR::Compiler->target.is64Bit())
         {
         uint8_t rex = toRealRegister(_classObjectRegister)->rexBits(TR::RealRegister::REX_B, false);
         if (rex)
            fixedLength++;
         }
      }
   else
      {
      int32_t delta = 0;
      delta = 1;
      TR::Compilation *comp = cg()->comp();
      uintptrj_t vftMask = TR::Compiler->om.maskOfObjectVftField();
      if (~vftMask != 0)
         delta += 6 + (TR::Compiler->target.is64Bit()? 1 /* Rex */ : 0);

      fixedLength = 8 + delta + (TR::Compiler->target.is64Bit()? 1 /* Rex */ : 0);
      }
   return fixedLength + estimateRestartJumpLength(estimatedSnippetStart + fixedLength);
   }







