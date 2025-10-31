/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "p/codegen/ForceRecompilationSnippet.hpp"

#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Relocation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/PPCRecompilation.hpp"
#include "ras/Logger.hpp"
#include "runtime/CodeCacheManager.hpp"

uint8_t *TR::PPCForceRecompilationSnippet::emitSnippetBody()
   {
   uint8_t             *buffer = cg()->getBinaryBufferCursor();
   TR::SymbolReference  *induceRecompilationSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_PPCinduceRecompilation);
   intptr_t startPC = (intptr_t)((uint8_t*)cg()->getCodeStart());
   // The relo runtime expects this kind of address to be zero in a relocatable binary, since it uses
   // |= to update the address chunks. See TR_PPC64RelocationTarget::storeAddressSequence.
   intptr_t startPCRegValue = cg()->needRelocationsForCurrentMethodStartPC() &&
                              !cg()->canEmitDataForExternallyRelocatableInstructions() ? 0 : startPC;

   getSnippetLabel()->setCodeLocation(buffer);

   TR::RegisterDependencyConditions *deps = _doneLabel->getInstruction()->getDependencyConditions();
   TR::RealRegister *startPCReg  = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(0)->getRealRegister());

   TR::InstOpCode opcode;

   if (cg()->comp()->target().is64Bit())
      {
      uint8_t *firstInstruction = buffer;

      // put jit entry point address in startPCReg
      uint32_t hhval, hlval, lhval, llval;
      hhval = startPCRegValue >> 48 & 0xffff;
      hlval = (startPCRegValue >>32) & 0xffff;
      lhval = (startPCRegValue >>16) & 0xffff;
      llval = startPCRegValue & 0xffff;

      opcode.setOpCodeValue(TR::InstOpCode::lis);
      buffer = opcode.copyBinaryToBuffer(buffer);
      startPCReg->setRegisterFieldRS((uint32_t *)buffer);
      *(uint32_t *)buffer |= hhval;
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::ori);
      buffer = opcode.copyBinaryToBuffer(buffer);
      startPCReg->setRegisterFieldRT((uint32_t *)buffer);
      startPCReg->setRegisterFieldRA((uint32_t *)buffer);
      *(uint32_t *)buffer |= hlval;
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::rldicr);
      buffer = opcode.copyBinaryToBuffer(buffer);
      startPCReg->setRegisterFieldRA((uint32_t *)buffer);
      startPCReg->setRegisterFieldRS((uint32_t *)buffer);
      // SH = 32;
      *(uint32_t *)buffer |= ((32 & 0x1f) << 11) | ((32 & 0x20) >> 4);
      // ME = 32
      *(uint32_t *)buffer |= 0x3e << 5;
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::oris);
      buffer = opcode.copyBinaryToBuffer(buffer);
      startPCReg->setRegisterFieldRA((uint32_t *)buffer);
      startPCReg->setRegisterFieldRS((uint32_t *)buffer);
      *(uint32_t *)buffer |= lhval;
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::ori);
      buffer = opcode.copyBinaryToBuffer(buffer);
      startPCReg->setRegisterFieldRA((uint32_t *)buffer);
      startPCReg->setRegisterFieldRS((uint32_t *)buffer);
      *(uint32_t *)buffer |= llval;
      buffer += PPC_INSTRUCTION_LENGTH;

      if (cg()->needRelocationsForCurrentMethodStartPC())
         {
         TR::Relocation *startPCRelo = new (cg()->trHeapMemory()) TR::ExternalRelocation(firstInstruction,
                                                                                        NULL,
                                                                                        (uint8_t *)fixedSequence1,
                                                                                        TR_StartPC,
                                                                                        cg());
         cg()->addExternalRelocation(startPCRelo,
                                     __FILE__,
                                     __LINE__,
                                     getNode());
         }
      }
   else
      {
      uint8_t *firstInstruction = buffer;

      // put jit entry point address in startPCReg
      opcode.setOpCodeValue(TR::InstOpCode::lis);
      buffer = opcode.copyBinaryToBuffer(buffer);
      startPCReg->setRegisterFieldRS((uint32_t *)buffer);
      *(uint32_t *)buffer |= (uint32_t) (startPCRegValue >> 16 & 0xffff);
      buffer += PPC_INSTRUCTION_LENGTH;

      uint8_t *secondInstruction = buffer;

      opcode.setOpCodeValue(TR::InstOpCode::ori);
      buffer = opcode.copyBinaryToBuffer(buffer);
      startPCReg->setRegisterFieldRT((uint32_t *)buffer);
      startPCReg->setRegisterFieldRA((uint32_t *)buffer);
      *(uint32_t *)buffer |= (uint32_t) (startPCRegValue & 0xffff);
      buffer += PPC_INSTRUCTION_LENGTH;

      if (cg()->needRelocationsForCurrentMethodStartPC())
         {
         TR_RelocationRecordInformation *recordInfo =
            (TR_RelocationRecordInformation *)cg()->comp()->trMemory()->allocateMemory(sizeof(TR_RelocationRecordInformation), heapAlloc);
         recordInfo->data1 = (uintptr_t)NULL;
         recordInfo->data3 = orderedPairSequence2;
         TR::Relocation *startPCRelo = new (cg()->trHeapMemory()) TR::ExternalOrderedPair32BitRelocation(firstInstruction,
                                                                                                         secondInstruction,
                                                                                                         (uint8_t *)recordInfo,
                                                                                                         TR_StartPC,
                                                                                                         cg());
         cg()->addExternalRelocation(startPCRelo,
                                     __FILE__,
                                     __LINE__,
                                     getNode());
         }

      }

   intptr_t helperAddress = (intptr_t)induceRecompilationSymRef->getMethodAddress();
   if (cg()->directCallRequiresTrampoline(helperAddress, (intptr_t)buffer))
      {
      helperAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(induceRecompilationSymRef->getReferenceNumber(), (void *)buffer);
      TR_ASSERT_FATAL(cg()->comp()->target().cpu.isTargetWithinIFormBranchRange(helperAddress, (intptr_t)buffer),
                      "Helper address is out of range");
      }

   // b distance
   *(int32_t *)buffer = 0x48000000 | ((helperAddress - (intptr_t)buffer) & 0x03ffffff);

   cg()->addExternalRelocation(
      TR::ExternalRelocation::create(
         buffer,
         (uint8_t *)induceRecompilationSymRef,
         TR_HelperAddress,
         cg()),
      __FILE__,
      __LINE__,
      getNode());

   buffer += PPC_INSTRUCTION_LENGTH;

   return buffer;
   }

void
TR_Debug::print(OMR::Logger *log, TR::PPCForceRecompilationSnippet * snippet)
   {
   uint8_t   *cursor = snippet->getSnippetLabel()->getCodeLocation();

   TR::RegisterDependencyConditions *deps = snippet->getDoneLabel()->getInstruction()->getDependencyConditions();
   TR::RealRegister *startPCReg  = _cg->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(0)->getRealRegister());

   printSnippetLabel(log, snippet->getSnippetLabel(), cursor, "EDO Recompilation Snippet");

   int32_t value;

   if (_comp->target().is64Bit())
      {
      printPrefix(log, NULL, cursor, 4);
      value = *((int32_t *) cursor) & 0x0ffff;
      log->printf("lis \t%s, 0x%p\t; Load jit entry point address", getName(startPCReg), value);
      cursor += 4;

      printPrefix(log, NULL, cursor, 4);
      value = *((int32_t *) cursor) & 0x0ffff;
      log->printf("ori \t%s, %s, 0x%p\t;", getName(startPCReg), getName(startPCReg), value);
      cursor += 4;

      printPrefix(log, NULL, cursor, 4);
      log->printf("rldicr \t%s, %s, 32, 31\t;", getName(startPCReg), getName(startPCReg));
      cursor += 4;

      printPrefix(log, NULL, cursor, 4);
      value = *((int32_t *) cursor) & 0x0ffff;
      log->printf("oris \t%s, %s, 0x%p\t;", getName(startPCReg), getName(startPCReg), value);
      cursor += 4;

      printPrefix(log, NULL, cursor, 4);
      value = *((int32_t *) cursor) & 0x0ffff;
      log->printf("ori \t%s, %s, 0x%p\t;", getName(startPCReg), getName(startPCReg), value);
      cursor += 4;
      }
   else
      {
      printPrefix(log, NULL, cursor, 4);
      value = *((int32_t *) cursor) & 0x0ffff;
      log->printf("lis \t%s, 0x%p\t; Load jit entry point address", getName(startPCReg), value);
      cursor += 4;

      printPrefix(log, NULL, cursor, 4);
      value = *((int32_t *) cursor) & 0x0ffff;
      log->printf("ori \t%s, %s, 0x%p\t;", getName(startPCReg), getName(startPCReg), value);
      cursor += 4;
      }

   const char *info = "";
   if (isBranchToTrampoline(_cg->getSymRef(TR_PPCinduceRecompilation), cursor, value))
      info = " Through trampoline";

   printPrefix(log, NULL, cursor, 4);
   value = *((int32_t *) cursor) & 0x03fffffc;
   value = (value << 6) >> 6;   // sign extend
   log->printf("b \t" POINTER_PRINTF_FORMAT "\t; %s%s", (intptr_t)cursor + value, getName(_cg->getSymRef(TR_PPCinduceRecompilation)), info);
   cursor += 4;
   }

uint32_t TR::PPCForceRecompilationSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return(cg()->comp()->target().is64Bit() ? 6*PPC_INSTRUCTION_LENGTH : 3*PPC_INSTRUCTION_LENGTH);
   }
