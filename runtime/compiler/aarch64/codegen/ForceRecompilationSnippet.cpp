/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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

#include "codegen/ARM64AOTRelocation.hpp"
#include "codegen/ARM64Instruction.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/ForceRecompilationSnippet.hpp"
#include "runtime/CodeCacheManager.hpp"

uint8_t *
TR::ARM64ForceRecompilationSnippet::emitSnippetBody()
   {
   uint8_t *cursor = cg()->getBinaryBufferCursor();

   TR::SymbolReference *induceRecompilationSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_ARM64induceRecompilation);
   intptr_t startPC = (intptr_t)(cg()->getCodeStart());

   getSnippetLabel()->setCodeLocation(cursor);

   TR::RegisterDependencyConditions *deps = _restartLabel->getInstruction()->getDependencyConditions();
   TR::RealRegister *startPCReg = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(0)->getRealRegister());

   // Put JIT entry point address in startPCReg
   uint32_t llval = startPC & 0xffff;
   uint32_t lhval = (startPC >> 16) & 0xffff;
   uint32_t hlval = (startPC >> 32) & 0xffff;
   uint32_t hhval = (startPC >> 48) & 0xffff;

   *(uint32_t *)cursor = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::movzx) | (llval << 5);
   startPCReg->setRegisterFieldRD((uint32_t *)cursor);
   cursor += ARM64_INSTRUCTION_LENGTH;
   *(uint32_t *)cursor = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::movkx) | ((lhval | TR::MOV_LSL16) << 5);
   startPCReg->setRegisterFieldRD((uint32_t *)cursor);
   cursor += ARM64_INSTRUCTION_LENGTH;
   *(uint32_t *)cursor = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::movkx) | ((hlval | TR::MOV_LSL32) << 5);
   startPCReg->setRegisterFieldRD((uint32_t *)cursor);
   cursor += ARM64_INSTRUCTION_LENGTH;
   *(uint32_t *)cursor = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::movkx) | ((hhval | TR::MOV_LSL48) << 5);
   startPCReg->setRegisterFieldRD((uint32_t *)cursor);
   cursor += ARM64_INSTRUCTION_LENGTH;

   intptr_t helperAddress = (intptr_t)induceRecompilationSymRef->getMethodAddress();
   intptr_t distance = helperAddress - (intptr_t)cursor;
   if (!constantIsSignedImm28(distance))
      {
      helperAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(induceRecompilationSymRef->getReferenceNumber(), (void *)cursor);
      distance = helperAddress - (intptr_t)cursor;
      TR_ASSERT_FATAL(constantIsSignedImm28(distance), "Trampoline too far away.");
      }

   // bl distance
   *(uint32_t *)cursor = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::bl) | ((distance >> 2) & 0x03ffffff); // imm26
   cg()->addExternalRelocation(
      TR::ExternalRelocation::create(
         cursor,
         (uint8_t *)induceRecompilationSymRef,
         TR_HelperAddress,
         cg()),
      __FILE__,
      __LINE__,
      getNode());
   cursor += ARM64_INSTRUCTION_LENGTH;

   if (_restartLabel != NULL)
      {
      distance = (intptr_t)(_restartLabel->getCodeLocation()) - (intptr_t)cursor;
      if (constantIsSignedImm28(distance))
         {
         // b distance
         *(uint32_t *)cursor = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::b) | ((distance >> 2) & 0x3ffffff); // imm26
         cursor += ARM64_INSTRUCTION_LENGTH;
         }
      else
         {
         TR_ASSERT_FATAL(false, "Target too far away.  Not supported yet");
         }
      }

   return cursor;
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARM64ForceRecompilationSnippet *snippet)
   {
   uint8_t *cursor = snippet->getSnippetLabel()->getCodeLocation();

   TR::LabelSymbol *restartLabel = snippet->getRestartLabel();
   TR::RegisterDependencyConditions *deps = restartLabel->getInstruction()->getDependencyConditions();
   TR::RealRegister *startPCReg = _cg->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(0)->getRealRegister());

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), cursor, "Force Recompilation Snippet");

   int32_t value;

   printPrefix(pOutFile, NULL, cursor, 4);
   value = (*((int32_t *)cursor) >> 5) & 0xffff;
   trfprintf(pOutFile, "movzx \t%s, 0x%04x\t; Load jit entry point address", getName(startPCReg), value);
   cursor += ARM64_INSTRUCTION_LENGTH;

   printPrefix(pOutFile, NULL, cursor, 4);
   value = (*((int32_t *)cursor) >> 5) & 0xffff;
   trfprintf(pOutFile, "movkx \t%s, 0x%04x, LSL #16", getName(startPCReg), value);
   cursor += ARM64_INSTRUCTION_LENGTH;

   printPrefix(pOutFile, NULL, cursor, 4);
   value = (*((int32_t *)cursor) >> 5) & 0xffff;
   trfprintf(pOutFile, "movkx \t%s, 0x%04x, LSL #32", getName(startPCReg), value);
   cursor += ARM64_INSTRUCTION_LENGTH;

   printPrefix(pOutFile, NULL, cursor, 4);
   value = (*((int32_t *)cursor) >> 5) & 0xffff;
   trfprintf(pOutFile, "movkx \t%s, 0x%04x, LSL #48", getName(startPCReg), value);
   cursor += ARM64_INSTRUCTION_LENGTH;

   char *info = "";
   if (isBranchToTrampoline(_cg->getSymRef(TR_ARM64induceRecompilation), cursor, value))
      info = " Through trampoline";

   printPrefix(pOutFile, NULL, cursor, 4);
   value = (*((int32_t *)cursor) & 0x3ffffff) << 2;
   value = (value << 4) >> 4; // sign extend
   trfprintf(pOutFile, "bl \t0x%p\t; %s%s", (intptr_t)cursor + value, getName(_cg->getSymRef(TR_ARM64induceRecompilation)), info);
   cursor += ARM64_INSTRUCTION_LENGTH;

   printPrefix(pOutFile, NULL, cursor, 4);
   value = (*((int32_t *)cursor) & 0x3ffffff) << 2;
   value = (value << 4) >> 4; // sign extend
   trfprintf(pOutFile, "b \t0x%p\t; Back to ", (intptr_t)cursor + value);
   print(pOutFile, restartLabel);
   }

uint32_t
TR::ARM64ForceRecompilationSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return 6 * ARM64_INSTRUCTION_LENGTH;
   }
