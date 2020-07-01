/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "p/codegen/PPCRecompilation.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/Machine.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/Snippet.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "env/VMJ9.h"
#include "il/DataTypes.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/PPCRecompilationSnippet.hpp"
#include "env/CompilerEnv.hpp"

// Allocate a machine-specific recompilation processor for this compilation
//
TR::Recompilation *TR_PPCRecompilation::allocate(TR::Compilation *comp)
   {
   if (comp->isRecompilationEnabled())
      {
      return new (comp->trHeapMemory()) TR_PPCRecompilation(comp);
      }

   return NULL;
   }

TR_PPCRecompilation::TR_PPCRecompilation(TR::Compilation * comp)
   : TR::Recompilation(comp)
   {
   _countingSupported = true;

   setupMethodInfo();
   }

TR_PersistentMethodInfo *TR_PPCRecompilation::getExistingMethodInfo(TR_ResolvedMethod *method)
   {
   int8_t *startPC = (int8_t *)method->startAddressForInterpreterOfJittedMethod();
   TR_PersistentMethodInfo *info = getJittedBodyInfoFromPC(startPC)->getMethodInfo();
   return(info);
   }

TR::Instruction *TR_PPCRecompilation::generatePrePrologue()
   {
   TR::Compilation *comp = cg()->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   // If in Full Speed Debug, allow to go through
   if (!couldBeCompiledAgain() && !comp->getOption(TR_FullSpeedDebug))
      return(NULL);

   // gr12 may contain the vtable offset, and must be preserved here
   // see PicBuilder.s and Recompilation.s
   TR::Instruction *cursor=NULL;
   TR::Machine *machine = cg()->machine();
   TR::Register   *gr0 = machine->getRealRegister(TR::RealRegister::gr0);
   TR::Node       *firstNode = comp->getStartTree()->getNode();
   TR::SymbolReference *recompileMethodSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_PPCsamplingRecompileMethod, false, false, false);
   TR_PersistentJittedBodyInfo *info = getJittedBodyInfo();
   // force creation of switch to interpreter pre prologue if in Full Speed Debug
   if (cg()->mustGenerateSwitchToInterpreterPrePrologue())
      {
      cursor = cg()->generateSwitchToInterpreterPrePrologue(cursor, firstNode);
      }

   if (useSampling() && couldBeCompiledAgain())
      {
      // gr0 must contain the saved LR; see Recompilation.s
      cursor = new (cg()->trHeapMemory()) TR::PPCTrg1Instruction(TR::InstOpCode::mflr, firstNode, gr0, cursor, cg());
      cursor = generateDepImmSymInstruction(cg(), TR::InstOpCode::bl, firstNode, (uintptr_t)recompileMethodSymRef->getMethodAddress(), new (cg()->trHeapMemory()) TR::RegisterDependencyConditions(0,0, cg()->trMemory()), recompileMethodSymRef, NULL, cursor);
      if (comp->target().is64Bit())
         {
         int32_t highBits = (int32_t)((intptr_t)info>>32), lowBits = (int32_t)((intptr_t)info);
         cursor = generateImmInstruction(cg(), TR::InstOpCode::dd, firstNode,
            comp->target().cpu.isBigEndian()?highBits:lowBits, TR_BodyInfoAddress, cursor);
         cursor = generateImmInstruction(cg(), TR::InstOpCode::dd, firstNode,
            comp->target().cpu.isBigEndian()?lowBits:highBits, cursor);
         }
      else
         {
         cursor = generateImmInstruction(cg(), TR::InstOpCode::dd, firstNode, (int32_t)(intptr_t)info, TR_BodyInfoAddress, cursor);
         }
      cursor = generateImmInstruction(cg(), TR::InstOpCode::dd, firstNode, 0, cursor);
      }

   return(cursor);
   }


TR::Instruction *TR_PPCRecompilation::generatePrologue(TR::Instruction *cursor)
   {
   if (couldBeCompiledAgain())
      {
      // gr12 may contain the vtable offset, and must be preserved here
      // see PicBuilder.s and Recompilation.s
      TR::Compilation *comp = cg()->comp();
      TR::Machine *machine = cg()->machine();
      TR::Register   *gr0 = machine->getRealRegister(TR::RealRegister::gr0);
      TR::Register   *gr11 = machine->getRealRegister(TR::RealRegister::gr11);
      TR::Register   *cr0 = machine->getRealRegister(TR::RealRegister::cr0);
      TR::Node       *firstNode = comp->getStartTree()->getNode();
      intptr_t        addr = (intptr_t)getCounterAddress();          // What is the RL category?
      TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg());

      if (comp->target().is64Bit())
         {
         intptr_t adjustedAddr = HI_VALUE(addr);
         // lis gr11, upper 16-bits
         cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::lis, firstNode, gr11, (int16_t)adjustedAddr>>32, cursor );
         // ori gr11, gr11, next 16-bit
         cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::ori, firstNode, gr11, gr11, ((adjustedAddr>>16) & 0x0000FFFF), cursor);
         // rldicr gr11, gr11, 32, 31
         cursor = generateTrg1Src1Imm2Instruction(cg(), TR::InstOpCode::rldicr, firstNode, gr11, gr11, 32, CONSTANT64(0xFFFFFFFF00000000), cursor);
         // oris  gr11, gr11, next 16-bits
         cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::oris, firstNode, gr11, gr11, (adjustedAddr & 0x0000FFFF), cursor);
         }
      else
         {
         cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::lis, firstNode, gr11, (int16_t)HI_VALUE(addr), cursor);
         }

         // lwzu  gr0, last 16-bits(gr11)
         cursor = generateTrg1MemInstruction(cg(), (!isProfilingCompilation())?TR::InstOpCode::lwzu:TR::InstOpCode::lwz, firstNode, gr0,
               new (cg()->trHeapMemory()) TR::MemoryReference(gr11, LO_VALUE(addr), 4, cg()), cursor);

      if (!isProfilingCompilation())
         {
         cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::addic_r, firstNode, gr0, gr0, -1, cursor);
         cursor = generateMemSrc1Instruction(cg(), TR::InstOpCode::stw, firstNode,
               new (cg()->trHeapMemory()) TR::MemoryReference(gr11, (int32_t)0, 4, cg()), gr0, cursor);
         }
      else
         {
         // This only applies to JitProfiling, as JProfiling uses sampling
         TR_ASSERT(comp->getProfilingMode() == JitProfiling, "JProfiling should not use counting mechanism to trip recompilation");

         cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::cmpi4, firstNode, cr0, gr0, 0, cursor);
         // this is just padding for consistent code length
         cursor = generateTrg1Src2Instruction   (cg(), TR::InstOpCode::OR,    firstNode, gr11,  gr11, gr11, cursor);
         }

      // gr0 must contain the saved LR; see Recompilation.s
      cursor = generateTrg1Instruction(cg(), TR::InstOpCode::mflr, firstNode, gr0, cursor);
      // this instruction is replaced after successful recompilation
      cursor = generateTrg1Src2Instruction   (cg(), TR::InstOpCode::OR,    firstNode, gr11,  gr11, gr11, cursor);
      cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::blt, firstNode, snippetLabel, cr0, cursor);
      TR::Snippet     *snippet = new (cg()->trHeapMemory()) TR::PPCRecompilationSnippet(snippetLabel, cursor->getPPCConditionalBranchInstruction(), cg());
      cg()->addSnippet(snippet);
      }
   return(cursor);
   }
