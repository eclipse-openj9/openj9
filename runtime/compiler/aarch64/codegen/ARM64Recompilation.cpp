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

#include "codegen/ARM64Recompilation.hpp"
#include "codegen/ARM64RecompilationSnippet.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GenerateInstructions.hpp"

TR_ARM64Recompilation::TR_ARM64Recompilation(TR::Compilation * comp)
   : TR::Recompilation(comp)
   {
   _countingSupported = true;

   setupMethodInfo();
   }

TR::Recompilation *TR_ARM64Recompilation::allocate(TR::Compilation *comp)
   {
   if (comp->isRecompilationEnabled())
      {
      return new (comp->trHeapMemory()) TR_ARM64Recompilation(comp);
      }

   return NULL;
   }

TR_PersistentMethodInfo *TR_ARM64Recompilation::getExistingMethodInfo(TR_ResolvedMethod *method)
   {
   int8_t *startPC = (int8_t *)method->startAddressForInterpreterOfJittedMethod();
   TR_PersistentMethodInfo *info = getJittedBodyInfoFromPC(startPC)->getMethodInfo();
   return info;
   }

TR::Instruction *TR_ARM64Recompilation::generatePrePrologue()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());

   // If in Full Speed Debug, allow to go through
   if (!couldBeCompiledAgain() && !_compilation->getOption(TR_FullSpeedDebug))
      return NULL;

   // x9 may contain the vtable offset, and must be preserved here
   // See PicBuilder.spp and Recompilation.spp
   TR::Instruction *cursor = NULL;
   TR::Machine *machine = cg()->machine();
   TR::Register *x8 = machine->getRealRegister(TR::RealRegister::x8);
   TR::Register *lr = machine->getRealRegister(TR::RealRegister::lr); // Link Register
   TR::Register *xzr = machine->getRealRegister(TR::RealRegister::xzr); // zero register
   TR::Node *firstNode = comp()->getStartTree()->getNode();
   TR::SymbolReference *recompileMethodSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_ARM64samplingRecompileMethod, false, false, false);
   TR_PersistentJittedBodyInfo *info = getJittedBodyInfo();

   // Force creation of switch to interpreter preprologue if in Full Speed Debug
   if (cg()->mustGenerateSwitchToInterpreterPrePrologue())
      {
      cursor = cg()->generateSwitchToInterpreterPrePrologue(cursor, firstNode);
      }

   if (useSampling() && couldBeCompiledAgain())
      {
      // x8 must contain the saved LR; see Recompilation.spp
      // cannot use generateMovInstruction() here
      cursor = new (cg()->trHeapMemory()) TR::ARM64Trg1Src2Instruction(TR::InstOpCode::orrx, firstNode, x8, xzr, lr, cursor, cg());
      cursor = generateImmSymInstruction(cg(), TR::InstOpCode::bl, firstNode,
                                         (uintptr_t)recompileMethodSymRef->getMethodAddress(),
                                         new (cg()->trHeapMemory()) TR::RegisterDependencyConditions(0, 0, cg()->trMemory()),
                                         recompileMethodSymRef, NULL, cursor);
      cursor = generateRelocatableImmInstruction(cg(), TR::InstOpCode::dd, firstNode, (uintptr_t)info, TR_BodyInfoAddress, cursor);

      // space for preserving original jitEntry instruction
      cursor = generateImmInstruction(cg(), TR::InstOpCode::dd, firstNode, 0, cursor);
      }

   return cursor;
   }

TR::Instruction *TR_ARM64Recompilation::generatePrologue(TR::Instruction *cursor)
   {
   TR::Recompilation *recomp = comp()->getRecompilationInfo();
   if (!recomp->useSampling())
      {
      // counting recompilation
      TR_UNIMPLEMENTED();
      }
   return cursor;
   }
