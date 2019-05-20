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

#include "x/codegen/X86Recompilation.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/PreprologueConst.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "x/codegen/RecompilationSnippet.hpp"
#include "x/codegen/X86Instruction.hpp"

// ***************************************************************************
//
// Implementation of TR_X86Recompilation
//
// ***************************************************************************

// Allocate a machine-specific recompilation processor for this compilation
//
TR::Recompilation *TR_X86Recompilation::allocate(TR::Compilation *comp)
   {
   if (comp->isRecompilationEnabled())
      {
      return new (comp->trHeapMemory()) TR_X86Recompilation(comp);
      }

   return NULL;
   }

TR_X86Recompilation::TR_X86Recompilation(TR::Compilation * comp)
   : TR::Recompilation(comp)
   {
   _countingSupported = true;

   setupMethodInfo();
   }

TR_PersistentMethodInfo *TR_X86Recompilation::getExistingMethodInfo(TR_ResolvedMethod *method)
   {
   // The method was previously compiled. Find its method info block from the
   // start PC address. The mechanism is different depending on whether the
   // method was compiled for sampling or counting.
   //
   void *startPC = method->startAddressForInterpreterOfJittedMethod();
   TR_PersistentMethodInfo *info = getMethodInfoFromPC(startPC);
   return info;
   }

TR::Instruction *TR_X86Recompilation::generatePrePrologue()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(_compilation->fe());

   if (!couldBeCompiledAgain())
      return NULL;

   TR::Node * startNode = _compilation->getStartTree()->getNode();
   TR::Instruction *prev = 0;

   uint8_t alignmentMargin = useSampling()? SAMPLING_CALL_SIZE /* allow for the helper call */ : 0;
   if (TR::Compiler->target.is64Bit())
      alignmentMargin += SAVE_AREA_SIZE +JITTED_BODY_INFO_SIZE + LINKAGE_INFO_SIZE; // save area for the first two bytes of the method + jitted body info pointer + linkageinfo
   else
      alignmentMargin += JITTED_BODY_INFO_SIZE + LINKAGE_INFO_SIZE; // jitted body info pointer + linkageinfo

   TR::LabelSymbol *startLabel = NULL;

   // Make sure the startPC at least 4-byte aligned.  This is important, since the VM
   // depends on the alignment (it uses the low order bits as tag bits).
   //
   int32_t alignmentBoundary = 8;
   if (cg()->mustGenerateSwitchToInterpreterPrePrologue())
      {
      // The SwitchToInterpreterPrePrologue will do the alignment for us.
      //
      prev = cg()->generateSwitchToInterpreterPrePrologue(prev, alignmentBoundary, alignmentMargin);
      }
   else
      {
      prev = generateAlignmentInstruction(prev, alignmentBoundary, alignmentMargin, cg());
      }

   if (TR::Compiler->target.is64Bit())
      {
      // A copy of the first two bytes of the method, in case we need to un-patch them
      //
      prev = new (trHeapMemory()) TR::X86ImmInstruction(prev, DWImm2, 0xcccc, cg());
      }

   if (useSampling())
      {
      // Note: the displacement of this call gets patched, but thanks to the
      // alignment constraint on the startPC, it is automatically aligned to a
      // 4-byte boundary, which is more than enough to ensure it can be patched
      // atomically.
      //
      prev = generateHelperCallInstruction(prev, SAMPLING_RECOMPILE_METHOD, cg());
      }

   // The address of the persistent method info is inserted in the pre-prologue
   // information. If the method is not to be compiled again a null value is
   // inserted.
   //
   if (TR::Compiler->target.is64Bit())
      {
      // TODO:AMD64: This ought to be a relative address, but that requires
      // binary-encoding-time support.  If you change this, be sure to adjust
      // the alignmentMargin above.
      //
      prev = new (trHeapMemory()) TR::AMD64Imm64Instruction(prev, DQImm64, (uintptrj_t)getJittedBodyInfo(), cg());
      prev->setNeedsAOTRelocation();
      }
   else
      {
      prev = new (trHeapMemory()) TR::X86ImmInstruction(prev, DDImm4, (uint32_t)(uintptrj_t)getJittedBodyInfo(), cg());
      prev->setNeedsAOTRelocation();
      }


   // Allow 4 bytes for private linkage return type info. Allocate the 4 bytes
   // even if the linkage is not private, so that all the offsets are
   // predictable.
   //
   return generateImmInstruction(DDImm4, startNode, 0, cg());
   }

TR::Instruction *TR_X86Recompilation::generatePrologue(TR::Instruction *cursor)
   {
   TR::Machine *machine = cg()->machine();
   TR::Linkage *linkage = cg()->getLinkage();
   if (couldBeCompiledAgain())
      {
      if (!useSampling())
         {
         // We're generating the first instruction ourselves.
         //
      TR::MemoryReference *mRef;

         if (TR::Compiler->target.is64Bit())
            {
            TR_ASSERT(linkage->getMinimumFirstInstructionSize() <= 10, "Can't satisfy first instruction size constraint");
            TR::RealRegister *scratchReg = machine->getRealRegister(TR::RealRegister::edi);
            cursor = new (trHeapMemory()) TR::AMD64RegImm64Instruction(cursor, MOV8RegImm64, scratchReg, (uintptrj_t)getCounterAddress(), cg());
            mRef = generateX86MemoryReference(scratchReg, 0, cg());
            }
         else
            {
            TR_ASSERT(linkage->getMinimumFirstInstructionSize() <= 5, "Can't satisfy first instruction size constraint");
            mRef = generateX86MemoryReference((intptrj_t)getCounterAddress(), cg());
            }

         if (!isProfilingCompilation())
            cursor = new (trHeapMemory()) TR::X86MemImmInstruction(cursor, SUB4MemImms, mRef, 1, cg());
         else
            {
            // This only applies to JitProfiling, as JProfiling uses sampling
            TR_ASSERT(_compilation->getProfilingMode() == JitProfiling, "JProfiling should not use counting mechanism to trigger recompilation");
            cursor = new (trHeapMemory()) TR::X86MemImmInstruction(cursor, CMP4MemImms, mRef, 0, cg());
            }

         TR::Instruction *counterInstruction = cursor;
         TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg());

         cursor = new (trHeapMemory()) TR::X86LabelInstruction(cursor, JL4, snippetLabel, cg());
         ((TR::X86LabelInstruction*)cursor)->prohibitShortening();
         TR::Snippet *snippet =
            new (trHeapMemory()) TR::X86RecompilationSnippet(snippetLabel, counterInstruction->getNode(), cg());
         cg()->addSnippet(snippet);
         }
      }

   return cursor;
   }

void TR_X86Recompilation::postCompilation()
   {
   setMethodReturnInfoBits();
   }

void TR_X86Recompilation::setMethodReturnInfoBits()
   {

   if (!couldBeCompiledAgain())
      return;

   // Sets up bits inside the linkage info field of the method.  Linkage info
   // is the dword immediately before the startPC
   //
   // The bits contain information about a) what kind of method header this
   // is (counting/sampling), and, b) what kind of
   // instruction was used as the first instruction.
   //
   // a) header type: the bits are used by getMethodInfoFromPC (above).
   //    the presence of bits guaranteed that the ptr to the PersistentMethodInfo
   //    would be located at the particular offset from startPC.
   //
   // b) In case a future compilation fails, we may need to restore the original first
   //    instruction of the method (this is done by OMR::Recompilation::methodCannotBeRecompiled)
   //
   uint8_t  *startPC = _compilation->cg()->getCodeStart();
   TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(startPC);

   if (useSampling())
      {
      linkageInfo->setSamplingMethodBody();
      saveFirstTwoBytes(startPC, START_PC_TO_ORIGINAL_ENTRY_BYTES);

      // TODO: It would be best if we didn't have to update the instruction object here, ideal
      // solution would be have these bits set as lazily as possible and remove them from consideration
      // prior to runtime, but race conditions for the update need to be considered.
      //
      if (comp()->getDebug())
         {
         cg()->getReturnTypeInfoInstruction()->setSourceImmediate(*(uint32_t*)linkageInfo);
         }
      }
   else
      {
      linkageInfo->setCountingMethodBody();
      }
   }

