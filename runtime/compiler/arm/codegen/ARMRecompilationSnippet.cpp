/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include "arm/codegen/ARMRecompilationSnippet.hpp"

#include <stdint.h>
#include "arm/codegen/ARMRecompilation.hpp"
#include "codegen/CodeGenerator.hpp"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"

uint8_t *TR::ARMRecompilationSnippet::emitSnippetBody()
   {
   /*
   Snippet will look like:
   bl    TR_ARMcountingRecompileMethod
   dd    jittedBodyInfo
   dd    code start location
   */

   uint8_t             *buffer = cg()->getBinaryBufferCursor();
   TR::SymbolReference  *countingRecompMethodSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_ARMcountingRecompileMethod, false, false, false);

   getSnippetLabel()->setCodeLocation(buffer);

   *(int32_t *)buffer = encodeHelperBranchAndLink(countingRecompMethodSymRef, buffer, getNode(), cg());  // BL resolve
   buffer += 4;

   *(int32_t *)buffer = (int32_t)(intptrj_t)cg()->comp()->getRecompilationInfo()->getJittedBodyInfo();
   buffer += 4;

   *(int32_t *)buffer = ((int32_t)(intptrj_t)cg()->getCodeStart());
   buffer += 4;

   return buffer;
   }

uint32_t TR::ARMRecompilationSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return  12;
   }
/*
uint8_t *TR::ARMEDORecompilationSnippet::emitSnippetBody()
   {
   uint8_t             *buffer = cg()->getBinaryBufferCursor();
   TR::SymbolReference  *induceRecompilationSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_ARMinduceRecompilation, false, false, false);
   intptrj_t startPC = (intptrj_t)((uint8_t*)cg()->getCodeStart());

   getSnippetLabel()->setCodeLocation(buffer);

   TR_ARMRegisterDependencyConditions *deps = _doneLabel->getInstruction()->getDependencyConditions();
   TR_ARMRealRegister *startPCReg  = getARMMachine(cg())->getARMRealRegister(deps->getPostConditions()->getRegisterDependency(0)->getRealRegister());

   TR_ARMOpCode opcode;

   if (TR::Compiler->target.is64Bit())
      {
      // put jit entry point address in startPCReg
      uint32_t hhval, hlval, lhval, llval;
      hhval = startPC >> 48 & 0xffff;
      hlval = (startPC >>32) & 0xffff;
      lhval = (startPC >>16) & 0xffff;
      llval = startPC & 0xffff;

      opcode.setOpCodeValue(ARMOp_lis);
      buffer = opcode.copyBinaryToBuffer(buffer);
      startPCReg->setRegisterFieldRS((uint32_t *)buffer);
      *(uint32_t *)buffer |= hhval;
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(ARMOp_ori);
      buffer = opcode.copyBinaryToBuffer(buffer);
      startPCReg->setRegisterFieldRT((uint32_t *)buffer);
      startPCReg->setRegisterFieldRA((uint32_t *)buffer);
      *(uint32_t *)buffer |= hlval;
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(ARMOp_rldicr);
      buffer = opcode.copyBinaryToBuffer(buffer);
      startPCReg->setRegisterFieldRA((uint32_t *)buffer);
      startPCReg->setRegisterFieldRS((uint32_t *)buffer);
      // SH = 32;
      *(uint32_t *)buffer |= ((32 & 0x1f) << 11) | ((32 & 0x20) >> 4);
      // ME = 32
      *(uint32_t *)buffer |= 0x3e << 5;
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(ARMOp_oris);
      buffer = opcode.copyBinaryToBuffer(buffer);
      startPCReg->setRegisterFieldRA((uint32_t *)buffer);
      startPCReg->setRegisterFieldRS((uint32_t *)buffer);
      *(uint32_t *)buffer |= lhval;
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(ARMOp_ori);
      buffer = opcode.copyBinaryToBuffer(buffer);
      startPCReg->setRegisterFieldRA((uint32_t *)buffer);
      startPCReg->setRegisterFieldRS((uint32_t *)buffer);
      *(uint32_t *)buffer |= llval;
      buffer += ARM_INSTRUCTION_LENGTH;
      }
   else
      {
      // put jit entry point address in startPCReg
      opcode.setOpCodeValue(ARMOp_lis);
      buffer = opcode.copyBinaryToBuffer(buffer);
      startPCReg->setRegisterFieldRS((uint32_t *)buffer);
      *(uint32_t *)buffer |= (uint32_t) (startPC >> 16 & 0xffff);
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(ARMOp_ori);
      buffer = opcode.copyBinaryToBuffer(buffer);
      startPCReg->setRegisterFieldRT((uint32_t *)buffer);
      startPCReg->setRegisterFieldRA((uint32_t *)buffer);
      *(uint32_t *)buffer |= (uint32_t) (startPC & 0xffff);
      buffer += ARM_INSTRUCTION_LENGTH;
      }

   intptrj_t distance = (intptrj_t)induceRecompilationSymRef->getMethodAddress() - (intptrj_t)buffer;
   if (!(distance>=BRANCH_BACKWARD_LIMIT && distance<=BRANCH_FORWARD_LIMIT))
      {
      distance = fej9->indexedTrampolineLookup(induceRecompilationSymRef->getReferenceNumber(), (void *)buffer) - (intptrj_t)buffer;
      TR_ASSERT(distance>=BRANCH_BACKWARD_LIMIT && distance<=BRANCH_FORWARD_LIMIT,
             "CodeCache is more than 32MB.\n");
      }

   // b distance
   *(int32_t *)buffer = 0x48000000 | (distance & 0x03ffffff);
   cg()->addExternalRelocation(new TR::ExternalRelocation(buffer,
                                                         (uint8_t *)induceRecompilationSymRef,
                                                         TR_HelperAddress), __FILE, __LINE__, getNode());
   buffer += ARM_INSTRUCTION_LENGTH;

   return buffer;
   }

uint32_t TR::ARMEDORecompilationSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return(TR::Compiler->target.is64Bit() ? 6*ARM_INSTRUCTION_LENGTH : 3*ARM_INSTRUCTION_LENGTH);
   }
*/
