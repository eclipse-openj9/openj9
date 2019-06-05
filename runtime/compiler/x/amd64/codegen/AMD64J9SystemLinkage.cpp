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

#include "codegen/AMD64J9SystemLinkage.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "x/codegen/J9LinkageUtils.hpp"

#if defined(TR_TARGET_64BIT)

////////////////////////////////////////////////
//
// Helpful definitions
//
// These are only here to make the rest of the code below somewhat
// self-documenting.
//
enum
   {
   RETURN_ADDRESS_SIZE=8,
   GPR_REG_WIDTH=8,
   AMD64_STACK_SLOT_SIZE=8,
   AMD64_DEFAULT_STACK_ALIGNMENT=16,
   AMD64_ABI_NUM_INTEGER_LINKAGE_REGS=6,
   AMD64_ABI_NUM_FLOAT_LINKAGE_REGS=8,
   WIN64_FAST_ABI_NUM_INTEGER_LINKAGE_REGS=4,
   WIN64_FAST_ABI_NUM_FLOAT_LINKAGE_REGS=4
   };


TR::AMD64J9Win64FastCallLinkage::AMD64J9Win64FastCallLinkage(TR::CodeGenerator *cg)
   : TR::AMD64SystemLinkage(cg), TR::AMD64J9SystemLinkage(cg), TR::AMD64Win64FastCallLinkage(cg)
   {
   _properties._framePointerRegister = TR::RealRegister::esp;
   _properties._methodMetaDataRegister = TR::RealRegister::ebp;
   }


TR::AMD64J9ABILinkage::AMD64J9ABILinkage(TR::CodeGenerator *cg)
   : TR::AMD64SystemLinkage(cg), TR::AMD64J9SystemLinkage(cg), TR::AMD64ABILinkage(cg)
   {
   _properties._framePointerRegister = TR::RealRegister::esp;
   _properties._methodMetaDataRegister = TR::RealRegister::ebp;
   }



TR::Register *
TR::AMD64J9SystemLinkage::buildVolatileAndReturnDependencies(
   TR::Node *callNode,
   TR::RegisterDependencyConditions *deps)
   {
   TR::Register* returnRegister = TR::AMD64SystemLinkage::buildVolatileAndReturnDependencies(callNode, deps);

   deps->addPostCondition(cg()->getVMThreadRegister(), TR::RealRegister::ebp, cg());

   deps->stopAddingPostConditions();

   return returnRegister;
   }


TR::Register *TR::AMD64J9SystemLinkage::buildDirectDispatch(
      TR::Node *callNode,
      bool spillFPRegs)
   {
#ifdef DEBUG
   if (debug("reportSysDispatch"))
      {
      printf("J9 AMD64 System Dispatch: %s calling %s\n", comp()->signature(), comp()->getDebug()->getName(callNode->getSymbolReference()));
      }
#endif

   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

   TR::Register *returnReg;

   TR::X86VFPDedicateInstruction *vfpDedicateInstruction =
      generateVFPDedicateInstruction(machine()->getRealRegister(getProperties().getIntegerScratchRegister(0)), callNode, cg());

   TR::J9LinkageUtils::switchToMachineCStack(callNode, cg());

   // Allocate adequate register dependencies.
   //
   // pre = number of argument registers
   // post = number of volatile + VMThread + return register
   //
   uint32_t pre = getProperties().getNumIntegerArgumentRegisters() + getProperties().getNumFloatArgumentRegisters();
   uint32_t post = getProperties().getNumVolatileRegisters() + 1 + (callNode->getDataType() == TR::NoType ? 0 : 1);
   TR::RegisterDependencyConditions *preDeps = generateRegisterDependencyConditions(pre, 0, cg());
   TR::RegisterDependencyConditions *postDeps = generateRegisterDependencyConditions(0, post, cg());

   // Evaluate outgoing arguments on the system stack and build pre-conditions.
   //
   int32_t memoryArgSize = buildArgs(callNode, preDeps);

   // Build post-conditions.
   //
   returnReg = buildVolatileAndReturnDependencies(callNode, postDeps);

   // Find the second scratch register in the post dependency list.
   //
   TR::Register *scratchReg = NULL;
   TR::RealRegister::RegNum scratchRegIndex = getProperties().getIntegerScratchRegister(1);
   for (int32_t i=0; i<post; i++)
      {
      if (postDeps->getPostConditions()->getRegisterDependency(i)->getRealRegister() == scratchRegIndex)
         {
         scratchReg = postDeps->getPostConditions()->getRegisterDependency(i)->getRegister();
         break;
         }
      }


   TR::Instruction *instr;
   if (methodSymbol->getMethodAddress())
      {
      TR_ASSERT(scratchReg, "could not find second scratch register");
      generateRegImm64Instruction(
         MOV8RegImm64,
         callNode,
         scratchReg,
         (uintptr_t)methodSymbol->getMethodAddress(),
         cg());

      instr = generateRegInstruction(CALLReg, callNode, scratchReg, preDeps, cg());
      }
   else
      {
      instr = generateImmSymInstruction(CALLImm4, callNode, (uintptrj_t)methodSymbol->getMethodAddress(), methodSymRef, preDeps, cg());
      }

   instr->setNeedsGCMap(getProperties().getPreservedRegisterMapForGC());

   cg()->stopUsingRegister(scratchReg);

   if (getProperties().getCallerCleanup() && memoryArgSize > 0)
      {
      // adjust sp is necessary, because for java, the stack is native stack, not java stack.
      // we need to restore native stack sp properly to the correct place.
      TR::RealRegister *espReal = machine()->getRealRegister(TR::RealRegister::esp);
      TR_X86OpCodes op = (memoryArgSize >= -128 && memoryArgSize <= 127) ? ADDRegImms() : ADDRegImm4();
      generateRegImmInstruction(op, callNode, espReal, memoryArgSize, cg());
      }

   if (returnReg && !(methodSymbol->isHelper()))
      TR::J9LinkageUtils::cleanupReturnValue(callNode, returnReg, returnReg, cg());

   TR::J9LinkageUtils::switchToJavaStack(callNode, cg());

   generateVFPReleaseInstruction(vfpDedicateInstruction, callNode, cg());

   TR::LabelSymbol *postDepLabel = generateLabelSymbol(cg());
   generateLabelInstruction(LABEL, callNode, postDepLabel, postDeps, cg());

   return returnReg;
   }

#endif

