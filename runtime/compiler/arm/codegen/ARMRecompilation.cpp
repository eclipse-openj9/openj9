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

#include "codegen/ARMRecompilation.hpp"

#include <stdint.h>
#include "arm/codegen/ARMRecompilationSnippet.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Snippet.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"

// Allocate a machine-specific recompilation processor for this compilation
//
TR::Recompilation *TR_ARMRecompilation::allocate(TR::Compilation *comp)
   {
   if (comp->isRecompilationEnabled())
      return new (comp->trHeapMemory()) TR_ARMRecompilation(comp);
   else
      return NULL;
   }

TR_ARMRecompilation::TR_ARMRecompilation(TR::Compilation * comp)
   : TR::Recompilation(comp)
   {
   _countingSupported = true;
   setupMethodInfo();
   }

TR_PersistentMethodInfo *TR_ARMRecompilation::getExistingMethodInfo(TR_ResolvedMethod *method)
   {
   int8_t *startPC = (int8_t *)method->startAddressForInterpreterOfJittedMethod();
   TR_PersistentMethodInfo *info = getJittedBodyInfoFromPC(startPC)->getMethodInfo();
   return(info);
   }

TR::Instruction *TR_ARMRecompilation::generatePrePrologue()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(_compilation->fe());
   // If in Full Speed Debug, allow to go through
   if (!couldBeCompiledAgain() && !_compilation->getOption(TR_FullSpeedDebug))
      return(NULL);

   // gr12 may contain the vtable offset, and must be preserved here
   // see PicBuilder.s and Recompilation.s
   TR::Instruction *cursor=NULL;
   TR::Machine *machine = cg()->machine();
   TR::Register   *gr4 = machine->getRealRegister(TR::RealRegister::gr4);
   TR::Register   *lr = machine->getRealRegister(TR::RealRegister::gr14); // link register
   TR::Node       *firstNode = _compilation->getStartTree()->getNode();
   TR::SymbolReference *recompileMethodSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_ARMsamplingRecompileMethod, false, false, false);
   TR_PersistentJittedBodyInfo *info = getJittedBodyInfo();


   // force creation of switch to interpreter pre prologue if in Full Speed Debug
   if (cg()->mustGenerateSwitchToInterpreterPrePrologue())
      {
      cursor = cg()->generateSwitchToInterpreterPrePrologue(cursor, firstNode);
      }

   if (couldBeCompiledAgain())
      {
      // gr4 must contain the saved LR; see Recompilation.s
      if(useSampling())
         {
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1Src1Instruction(cursor, ARMOp_mov, firstNode, gr4, lr, cg());
         cursor = generateImmSymInstruction(cg(), ARMOp_bl, firstNode, (uintptrj_t)recompileMethodSymRef->getMethodAddress(), new (cg()->trHeapMemory()) TR::RegisterDependencyConditions((uint8_t)0, 0, cg()->trMemory()), recompileMethodSymRef, NULL, cursor);
         }
      cursor = new (cg()->trHeapMemory()) TR::ARMImmInstruction(cursor, ARMOp_dd, firstNode, (int32_t)(intptrj_t)info, cg());
      cursor->setNeedsAOTRelocation();
      ((TR::ARMImmInstruction *)cursor)->setReloKind(TR_BodyInfoAddress);

      cursor = generateImmInstruction(cg(), ARMOp_dd, firstNode, 0, cursor);
      }
   return(cursor);
   }

TR::Instruction *TR_ARMRecompilation::generatePrologue(TR::Instruction *cursor)
   {
   if (couldBeCompiledAgain())
      {
      // gr12 may contain the vtable offset, and must be preserved here
      // see PicBuilder.s and Recompilation.s
      TR::Machine *machine = cg()->machine();
      TR::Register   *gr4 = machine->getRealRegister(TR::RealRegister::gr4);
      TR::Register   *gr5 = machine->getRealRegister(TR::RealRegister::gr5);
      TR::Register   *lr = machine->getRealRegister(TR::RealRegister::gr14); // link register
      TR::Node       *firstNode = _compilation->getStartTree()->getNode();
      intptrj_t        addr = (intptrj_t)getCounterAddress();
      TR::LabelSymbol *snippetLabel = TR::LabelSymbol::create(cg()->trHeapMemory(), cg());
      intParts localVal(addr);

      TR_ARMOperand2 *op2_3 = new (cg()->trHeapMemory()) TR_ARMOperand2(localVal.getByte3(), 24);
      TR_ARMOperand2 *op2_2 = new (cg()->trHeapMemory()) TR_ARMOperand2(localVal.getByte2(), 16);
      TR_ARMOperand2 *op2_1 = new (cg()->trHeapMemory()) TR_ARMOperand2(localVal.getByte1(), 8);
      TR_ARMOperand2 *op2_0 = new (cg()->trHeapMemory()) TR_ARMOperand2(localVal.getByte0(), 0);
      cursor = generateTrg1Src1Instruction(cg(), ARMOp_mov, firstNode, gr5, op2_3, cursor);
      cursor = generateTrg1Src2Instruction(cg(), ARMOp_add, firstNode, gr5, gr5, op2_2, cursor);
      cursor = generateTrg1Src2Instruction(cg(), ARMOp_add, firstNode, gr5, gr5, op2_1, cursor);
      cursor = generateTrg1Src2Instruction(cg(), ARMOp_add, firstNode, gr5, gr5, op2_0, cursor);

      cursor = generateTrg1MemInstruction(cg(), ARMOp_ldr, firstNode, gr4, new (cg()->trHeapMemory()) TR::MemoryReference(gr5, 0, cg()), cursor);

      if (!isProfilingCompilation())
         {
         cursor = generateTrg1Src1ImmInstruction(cg(), ARMOp_sub_r, firstNode, gr4, gr4, 1, 0, cursor);
         cursor = generateMemSrc1Instruction(cg(), ARMOp_str, firstNode,
                                new (cg()->trHeapMemory()) TR::MemoryReference(gr5, (int32_t)0, cg()), gr4, cursor);
         }
      else
         {
         // This only applies to JitProfiling, as JProfiling uses sampling
         TR_ASSERT(_compilation->getProfilingMode() == JitProfiling, "JProfiling should use sampling to trigger recompilation");

         cursor = generateSrc1ImmInstruction(cg(), ARMOp_cmp, firstNode, gr4, 0, 0, cursor);
         // this is just padding for consistent code length
         cursor = generateTrg1Src2Instruction(cg(), ARMOp_orr, firstNode, gr5, gr5, gr5, cursor);
         }

      // gr4 must contain the saved LR; see Recompilation.s
      cursor = generateTrg1Src1Instruction(cg(), ARMOp_mov, firstNode, gr4, lr, cursor);
      // the instruction below is replaced after successful recompilation
      cursor = generateTrg1Src2Instruction(cg(), ARMOp_orr, firstNode, gr5, gr5, gr5, cursor);
      cursor = generateConditionalBranchInstruction(cg(), firstNode, ARMConditionCodeLT, snippetLabel, cursor);

      TR::Snippet     *snippet = new (cg()->trHeapMemory()) TR::ARMRecompilationSnippet(snippetLabel, cursor, cg());
      cg()->addSnippet(snippet);
      }
   return(cursor);
   }

