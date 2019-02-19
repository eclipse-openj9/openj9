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

#include "z/codegen/J9S390Snippet.hpp"

#include <stdint.h>
#include "j9.h"
#include "thrdsup.h"
#include "thrtypes.h"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/Machine.hpp"
#include "codegen/SnippetGCMap.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "env/VMJ9.h"
#include "runtime/CodeCacheManager.hpp"
#include "z/codegen/J9S390PrivateLinkage.hpp"
#include "z/codegen/S390Instruction.hpp"
#include "z/codegen/S390Snippets.hpp"

TR::S390HeapAllocSnippet::S390HeapAllocSnippet(
   TR::CodeGenerator   *codeGen,
   TR::Node            *node,
   TR::LabelSymbol      *callLabel,
   TR::SymbolReference *destination,
   TR::LabelSymbol      *restartLabel)
   : _restartLabel(restartLabel), _destination(destination), _isLongBranch(false), _length(0),
      TR::Snippet(codeGen, node, callLabel, destination->canCauseGC())
   {
   if (destination->canCauseGC())
      {
      // Helper call, preserves all registers
      //
      gcMap().setGCRegisterMask(0x0000FFFF);
      }
   }


uint8_t *
TR::S390HeapAllocSnippet::emitSnippetBody()
   {

   TR::CodeGenerator * codeGen = cg();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(codeGen->fe());
   uint8_t * buffer = codeGen->getBinaryBufferCursor();
   int32_t distance;
   int32_t jumpToCallDistance = -1;
   intptrj_t branchToCallLocation = -1;

   TR::Machine *machine = codeGen->machine();
   TR::RegisterDependencyConditions *deps = getRestartLabel()->getInstruction()->getDependencyConditions();
   TR::RealRegister * resReg = machine->getRealRegister(deps->getPostConditions()->getRegisterDependency(2)->getRealRegister());
   uint32_t resRegEncoding = resReg->getRegisterNumber() - 1;
   bool is64BitTarget = TR::Compiler->target.is64Bit();

   getSnippetLabel()->setCodeLocation(buffer);

   TR_ASSERT(jumpToCallDistance == -1 || jumpToCallDistance == (((intptrj_t)buffer) - branchToCallLocation), "Jump in Heap Alloc Misaligned.");

   // The code for the none-G5 32bit snippet looks like:
   // BRASL  gr14, jitNewXXXX;
   // LR/LGR     resReg, gr2;
   // BRC[L] restartLabel;

   *(uint16_t *) buffer = 0xc0e5;
   buffer += 2;

   intptrj_t destAddr = (intptrj_t) getDestination()->getSymbol()->castToMethodSymbol()->getMethodAddress();

#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390)
   if (cg()->comp()->getOption(TR_EnableRMODE64))
#endif
      {
      if (NEEDS_TRAMPOLINE(destAddr, buffer, cg()))
         {
         uint32_t rEP = (uint32_t) cg()->getEntryPointRegister() - 1;
         // Destination is beyond our reachable jump distance, we'll find the
         // trampoline.
         destAddr = TR::CodeCacheManager::instance()->findHelperTrampoline(getDestination()->getReferenceNumber(), (void *)buffer);
         this->setUsedTrampoline(true);

         // We clobber rEP if we take a trampoline.  Update our register map if necessary.
         if (gcMap().getStackMap() != NULL)
            {
            gcMap().getStackMap()->maskRegisters(~(0x1 << (rEP)));
            }
         }
      }
#endif

   TR_ASSERT(CHECK_32BIT_TRAMPOLINE_RANGE(destAddr, buffer), "Helper Call is not reachable.");
   this->setSnippetDestAddr(destAddr);

   *(int32_t *) buffer = (int32_t)((destAddr - (intptrj_t)(buffer - 2)) / 2);
   if (cg()->comp()->compileRelocatableCode())
      {
      cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(buffer, (uint8_t*) getDestination(), TR_HelperAddress, cg()),
                                __FILE__, __LINE__, getNode());
      }

   buffer += sizeof(int32_t);

   if (gcMap().getStackMap() != NULL)
      {
      gcMap().getStackMap()->resetRegistersBits(codeGen->registerBitMask(resReg->getRegisterNumber()));
      }

   gcMap().registerStackMap(buffer, cg());

   if (is64BitTarget)
      {
      *(uint32_t *) buffer = 0xb9040002 | (resRegEncoding << 4);
      buffer += 4;
      }
   else
      {
      *(uint16_t *) buffer = 0x1802 | (resRegEncoding << 4);
      buffer += 2;
      }

   distance = (intptrj_t) getRestartLabel()->getCodeLocation() - (intptrj_t) buffer;
   distance >>= 1;
   if (!isLongBranch())
      {
      *(int32_t *) buffer = 0xa7f40000 | (distance & 0x0000ffff);
      buffer += 4;
      }
   else
      {
      *(uint16_t *) buffer = 0xc0f4;
      buffer += 2;
      *(int32_t *) buffer = distance;
      buffer += 4;
      }

   return buffer;
   }

uint32_t
TR::S390HeapAllocSnippet::getLength(int32_t  estimatedCodeStart)
   {
   TR::CodeGenerator * codeGen = cg();
   int32_t distance;
   uint32_t length = getMyLength();

   if (length != 0)
      {
      return length;
      }

   distance = estimatedCodeStart +
        (TR::Compiler->target.is64Bit() ? 10 : 8) -
        getRestartLabel()->getEstimatedCodeLocation();

   // to be conservative: we use the byte count as half-word count, accounting for length estimation deviation
   if (distance<MIN_IMMEDIATE_VAL || distance>MAX_IMMEDIATE_VAL)
      {
      setIsLongBranch(true);
      }
   if (TR::Compiler->target.is64Bit())
      {
      if (isLongBranch())
         length = 16;
      else
         length = 14;
      }
   else
      {
      if (isLongBranch())
         length = 14;
      else
         length = 12;
      }

   setMyLength(length);

   return length;
   }

void
TR::S390HeapAllocSnippet::print(TR::FILE *pOutFile, TR_Debug *debug)
   {
   if (pOutFile == NULL)
      {
      return;
      }

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->comp()->fe());
   uint8_t * buffer = getSnippetLabel()->getCodeLocation();
   int32_t distance;
   bool is64BitTarget = TR::Compiler->target.is64Bit();

   TR::Machine *machine = cg()->machine();
   TR::RegisterDependencyConditions *deps = getRestartLabel()->getInstruction()->getDependencyConditions();
   TR::RealRegister * resReg = machine->getRealRegister(deps->getPostConditions()->getRegisterDependency(2)->getRealRegister());

   debug->printSnippetLabel(pOutFile, getSnippetLabel(), buffer, "HeapAlloc Snippet", debug->getName(getDestination()));


   // The code for the snippet looks like:
   // BRASL  gr14, jitNewXXXX;
   // LR/LGR     resReg, gr2;
   // BRC[L] restartLabel;

   distance = (intptrj_t) getDestination()->getSymbol()->castToMethodSymbol()->getMethodAddress() - (intptrj_t) buffer;

   debug->printPrefix(pOutFile, NULL, buffer, 6);
   trfprintf(pOutFile, "BRASL \tGPR14, 0x%8x", distance >> 1);
   buffer += 6;

   if (TR::Compiler->target.is64Bit())
      {
      debug->printPrefix(pOutFile, NULL, buffer, 4);
      trfprintf(pOutFile, "LGR \t%s,GPR2", debug->getName(resReg));
      buffer += 4;
      }
   else
      {
      debug->printPrefix(pOutFile, NULL, buffer, 2);
      trfprintf(pOutFile, "LR \t%s, GPR2", debug->getName(resReg));
      buffer += 2;
      }

   distance = (intptrj_t) getRestartLabel()->getCodeLocation() - (intptrj_t) buffer;
   distance >>= 1;
   if (!isLongBranch())
      {
      debug->printPrefix(pOutFile, NULL, buffer, 4);
      trfprintf(pOutFile, "BRC \t");
      debug->print(pOutFile, getRestartLabel());
      buffer += 4;
      }
   else
      {
      debug->printPrefix(pOutFile, NULL, buffer, 6);
      trfprintf(pOutFile, "BRCL \t");
      debug->print(pOutFile, getRestartLabel());
      buffer += 6;
      }
   }

uint32_t
TR::S390RestoreGPR7Snippet::getLength(int32_t estimatedSnippetStart)
   {

   if (TR::Compiler->target.is64Bit())
#if defined(J9VM_JIT_FREE_SYSTEM_STACK_POINTER)
      return 6*3 + 6 + 4;
#else
      return 6*1 + 6 + 4;
#endif
   else
#if defined(J9VM_JIT_FREE_SYSTEM_STACK_POINTER)
      return 4*3 + 6 + 4;
#else
      return 4*1 + 6 + 4;
#endif
   }

uint8_t *
TR::S390RestoreGPR7Snippet::emitSnippetBody()
   {
   uint8_t * cursor = cg()->getBinaryBufferCursor();
   uint8_t * startPointCursor = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(cursor);
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());

   int32_t tempOffset =  fej9->thisThreadGetTempSlotOffset();
#if defined(J9VM_JIT_FREE_SYSTEM_STACK_POINTER)
   int32_t returnOffset = fej9->thisThreadGetReturnValueOffset();
   int32_t sspOffset = fej9->thisThreadGetSystemSPOffset();
#endif


   //E370DXXX0058
   if (TR::Compiler->target.is64Bit())//generate LG
      {
#if defined(J9VM_JIT_FREE_SYSTEM_STACK_POINTER)
      *(int32_t *) cursor = 0xE340D000|sspOffset;
      cursor += sizeof(int32_t);
      *(int16_t *) cursor = 0x0024;
      cursor += sizeof(int16_t);
#endif

      //restore gpr7 from tempslot, 6bytes
      *(int32_t *) cursor = 0xE370D000|tempOffset;
      cursor += sizeof(int32_t);
      *(int16_t *) cursor = 0x0004;
      cursor += sizeof(int16_t);

#if defined(J9VM_JIT_FREE_SYSTEM_STACK_POINTER)
      //restore gpr4 from returnValue, 6 bytes
      *(int32_t *) cursor = 0xE340D000|returnOffset;
      cursor += sizeof(int32_t);
      *(int16_t *) cursor = 0x0004;
      cursor += sizeof(int16_t);
#endif
      }
   else
      { //generate L
#if defined(J9VM_JIT_FREE_SYSTEM_STACK_POINTER)
      TR_ASSERT(tempOffset < 4096 && returnOffset < 4096, "if > 4k then must use LY, not L.");
#else
      TR_ASSERT(tempOffset < 4096, "if > 4k then must use LY, not L.");
#endif

#if defined(J9VM_JIT_FREE_SYSTEM_STACK_POINTER)
      //store GPR4 onto vmThread->ssp
      *(int32_t *) cursor = 0x5040D000|(sspOffset & 0x0FFF);
      cursor += sizeof(int32_t);
#endif

      //gpr7, 4bytes
      *(int32_t *) cursor = 0x5870D000|(tempOffset & 0x0FFF);
      cursor += sizeof(int32_t);

#if defined(J9VM_JIT_FREE_SYSTEM_STACK_POINTER)
      //gpr4, 4bytes
      *(int32_t *) cursor = 0x5840D000|(returnOffset & 0x0FFF);
      cursor += sizeof(int32_t);
#endif
      }


   cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, getTargetLabel()));
   // 6 bytes
   // BRCL 0xf, <target>
   *(int16_t *) cursor = 0xC0F4;
   cursor += sizeof(int16_t);

   // BRCL - Add Relocation the data constants to this LARL.
   *(int32_t *) cursor = 0xDEADBEEF;
   cursor += sizeof(int32_t);

#if defined(J9VM_JIT_FREE_SYSTEM_STACK_POINTER)
    if (TR::Compiler->target.is64Bit())
        TR_ASSERT(((U_8*)cursor) - startPointCursor == 24,
                "for 64bit, DAA eyecatcher should be generated at offset 24 bytes");
    else
        TR_ASSERT(((U_8*)cursor) - startPointCursor == 18,
                "for 31bit, DAA eyecatcher should be generated at offset 18 bytes");
#endif

   *(int32_t *) cursor = 0xDAA0CA11;
   cursor += sizeof(int32_t);

   return cursor;
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390RestoreGPR7Snippet *snippet)
   {
   uint8_t * buffer = snippet->getSnippetLabel()->getCodeLocation();
   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), buffer, "Restore GPR 7 after signal handler");

#ifdef J9VM_JIT_FREE_SYSTEM_STACK_POINTER
   //save GPR4 to
   if (TR::Compiler->target.is64Bit())
      {
      printPrefix(pOutFile, NULL, buffer, 6);
      trfprintf(pOutFile, "STG   GPR7, SSP(GPR13)", snippet->getTargetLabel()->getCodeLocation());
      buffer += 6;
      }
   else
      {
      printPrefix(pOutFile, NULL, buffer, 4);
      trfprintf(pOutFile, "ST    GPR7, SSP(GPR13)", snippet->getTargetLabel()->getCodeLocation());
      buffer += 4;
      }
#endif

   //0x4a2af68a (???)       e370d0980058 LY      R7,152(,R13)
   if (TR::Compiler->target.is64Bit())
      {
      printPrefix(pOutFile, NULL, buffer, 6);
      trfprintf(pOutFile, "LG    GPR7, 152(GPR13)", snippet->getTargetLabel()->getCodeLocation());
      buffer += 6;
      }
   else
      {
      printPrefix(pOutFile, NULL, buffer, 4);
      trfprintf(pOutFile, "L     GPR7, 144(GPR13)", snippet->getTargetLabel()->getCodeLocation());
      buffer += 4;
      }

#ifdef J9VM_JIT_FREE_SYSTEM_STACK_POINTER
   //                                    L      R4,136(,R13)
   if (TR::Compiler->target.is64Bit())
      {
      printPrefix(pOutFile, NULL, buffer, 6);
      trfprintf(pOutFile, "LG    GPR4, 136(GPR13)", snippet->getTargetLabel()->getCodeLocation());
      buffer += 6;
      }
   else
      {
      printPrefix(pOutFile, NULL, buffer, 4);
      trfprintf(pOutFile, "L     GPR4, 136(GPR13)", snippet->getTargetLabel()->getCodeLocation());
      buffer += 4;
      }
#endif

   //0x4a2af690 (???)       c0f4fff1bb80 BRCL    15,*-1870080
   printPrefix(pOutFile, NULL, buffer, 6);
   trfprintf(pOutFile, "BRCL  0xF, targetLabel(%p)", snippet->getTargetLabel()->getCodeLocation());
   buffer += 6;

   printPrefix(pOutFile, NULL, buffer, 4);
   trfprintf(pOutFile, "Eye Catcher = 0xDAA0CA11", snippet->getTargetLabel()->getCodeLocation());
   buffer += 4;
   }

