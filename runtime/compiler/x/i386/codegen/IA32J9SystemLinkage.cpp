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

#if defined(TR_TARGET_32BIT)

#include "codegen/IA32J9SystemLinkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/X86Instruction.hpp"
#include "x/codegen/IA32LinkageUtils.hpp"
#include "codegen/LiveRegister.hpp"
#include "x/codegen/J9LinkageUtils.hpp"
#include "codegen/CodeGenerator.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

TR::IA32J9SystemLinkage::IA32J9SystemLinkage(TR::CodeGenerator *cg) : TR::IA32SystemLinkage(cg)
   {
   _properties._framePointerRegister = TR::RealRegister::ebx;
   _properties._methodMetaDataRegister = TR::RealRegister::ebp;
   }


TR::Register*
TR::IA32J9SystemLinkage::buildVolatileAndReturnDependencies(TR::Node *callNode, TR::RegisterDependencyConditions *deps)
   {
   TR::Register* returnReg = TR::IA32SystemLinkage::buildVolatileAndReturnDependencies(callNode, deps);

   deps->addPostCondition(cg()->getMethodMetaDataRegister(), TR::RealRegister::ebp, cg());

   uint8_t numXmmRegs = 0;
   TR_LiveRegisters *lr = cg()->getLiveRegisters(TR_FPR);
   if (!lr || lr->getNumberOfLiveRegisters() > 0)
      numXmmRegs = (uint8_t)(TR::RealRegister::LastXMMR - TR::RealRegister::FirstXMMR+1);

   if (numXmmRegs > 0)
      {
      for (int regIndex = TR::RealRegister::FirstXMMR; regIndex <= TR::RealRegister::LastXMMR; regIndex++)
         {
         TR::Register *dummy = cg()->allocateRegister(TR_FPR);
         dummy->setPlaceholderReg();
         deps->addPostCondition(dummy, (TR::RealRegister::RegNum)regIndex, cg());
         cg()->stopUsingRegister(dummy);
         }
      }

   deps->stopAddingConditions();
   return returnReg;
   }



TR::Register*
TR::IA32J9SystemLinkage::buildDirectDispatch(TR::Node *callNode, bool spillFPRegs)
   {
#ifdef DEBUG
   if (debug("reportSysDispatch"))
      {
      printf("System Linkage Dispatch: %s calling %s\n", comp()->signature(), comp()->getDebug()->getName(callNode->getSymbolReference()));
      }
#endif

   TR::RealRegister    *espReal      = machine()->getRealRegister(TR::RealRegister::esp);
   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = callNode->getSymbol()->castToMethodSymbol();
   uint8_t numXmmRegs = 0;

   if (!methodSymbol->isHelper())
      diagnostic("Building call site for %s\n", methodSymbol->getMethod()->signature(trMemory()));

   int32_t argSize = buildParametersOnCStack(callNode, 0);

   TR::LabelSymbol* begLabel = generateLabelSymbol(cg());
   TR::LabelSymbol* endLabel = generateLabelSymbol(cg());
   begLabel->setStartInternalControlFlow();
   endLabel->setEndInternalControlFlow();

   generateLabelInstruction(LABEL, callNode, begLabel, cg());

   // Save VFP
   TR::X86VFPSaveInstruction* vfpSave = generateVFPSaveInstruction(callNode, cg());

   TR::J9LinkageUtils::switchToMachineCStack(callNode, cg());

   // check to see if we need to set XMM register dependencies
   TR_LiveRegisters *lr = cg()->getLiveRegisters(TR_FPR);
   if (!lr || lr->getNumberOfLiveRegisters() > 0)
      numXmmRegs = (uint8_t)(TR::RealRegister::LastXMMR - TR::RealRegister::FirstXMMR+1);

   TR::RegisterDependencyConditions  *deps;
   deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)6 + numXmmRegs, cg());
   TR::Register *returnReg = buildVolatileAndReturnDependencies(callNode, deps);

   TR::RegisterDependencyConditions  *dummy = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)0, cg());

   // Call-out
   generateRegImmInstruction(argSize >= -128 && argSize <= 127 ? SUB4RegImms : SUB4RegImm4, callNode, espReal, argSize, cg());
   generateImmSymInstruction(CALLImm4, callNode, (uintptr_t)methodSymbol->getMethodAddress(), methodSymRef, cg());

   if (returnReg && !(methodSymbol->isHelper()))
      TR::J9LinkageUtils::cleanupReturnValue(callNode, returnReg, returnReg, cg());

   TR::J9LinkageUtils::switchToJavaStack(callNode, cg());

   // Restore VFP
   generateVFPRestoreInstruction(vfpSave, callNode, cg());
   generateLabelInstruction(LABEL, callNode, endLabel, deps, cg());

   // Stop using the killed registers that are not going to persist
   //
   if (deps)
      stopUsingKilledRegisters(deps, returnReg);

   if (callNode->getOpCode().isFloat())
      {
      TR::MemoryReference  *tempMR = machine()->getDummyLocalMR(TR::Float);
      generateFPMemRegInstruction(FSTPMemReg, callNode, tempMR, returnReg, cg());
      cg()->stopUsingRegister(returnReg); // zhxingl: this is added by me
      returnReg = cg()->allocateSinglePrecisionRegister(TR_FPR);
      generateRegMemInstruction(MOVSSRegMem, callNode, returnReg, generateX86MemoryReference(*tempMR, 0, cg()), cg());
      }
   else if (callNode->getOpCode().isDouble())
      {
      TR::MemoryReference  *tempMR = machine()->getDummyLocalMR(TR::Double);
      generateFPMemRegInstruction(DSTPMemReg, callNode, tempMR, returnReg, cg());
      cg()->stopUsingRegister(returnReg); // zhxingl: this is added by me
      returnReg = cg()->allocateRegister(TR_FPR);
      generateRegMemInstruction(cg()->getXMMDoubleLoadOpCode(), callNode, returnReg, generateX86MemoryReference(*tempMR, 0, cg()), cg());
      }

   if (cg()->enableRegisterAssociations())
      associatePreservedRegisters(deps, returnReg);

   return returnReg;
   }

int32_t TR::IA32J9SystemLinkage::buildParametersOnCStack(TR::Node *callNode, int firstParamIndex, bool passVMThread, bool wrapAddress)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   TR_Stack<TR::MemoryReference*> paramsSlotsOnStack(trMemory(), callNode->getNumChildren()*2, false, stackAlloc);
   // Evaluate children that are not part of parameters
   for (size_t i = 0; i < firstParamIndex; i++)
      {
      auto child = callNode->getChild(i);
      cg()->evaluate(child);
      cg()->decReferenceCount(child);
      }
   // Load C Stack Pointer
   auto cSP = cg()->allocateRegister();
   generateRegMemInstruction(L4RegMem, callNode, cSP, generateX86MemoryReference(cg()->getVMThreadRegister(), fej9->thisThreadGetMachineSPOffset(), cg()), cg());
   // Pass in the env to the jni method as the lexically first arg.
   if (passVMThread)
      {
      auto slot = generateX86MemoryReference(cSP, 0, cg());
      generateMemRegInstruction(S4MemReg, callNode, slot, cg()->getVMThreadRegister(), cg());
      paramsSlotsOnStack.push(slot);
      }
   // Evaluate params
   for (size_t i = firstParamIndex; i < callNode->getNumChildren(); i++)
      {
      auto child = callNode->getChild(i);
      auto param = cg()->evaluate(child);
      auto slot = generateX86MemoryReference(cSP, 0, cg());
      paramsSlotsOnStack.push(slot);
      switch (child->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
            generateMemRegInstruction(S4MemReg, callNode, slot, param, cg());
            break;
         case TR::Address:
            if (wrapAddress && child->getOpCodeValue() == TR::loadaddr)
               {
               TR::StaticSymbol* sym = child->getSymbolReference()->getSymbol()->getStaticSymbol();
               if (sym && sym->isAddressOfClassObject())
                  {
                  generateMemRegInstruction(S4MemReg, callNode, slot, param, cg());
                  }
               else // must be loadaddr of parm or local
                  {
                  TR::Register *tmp = cg()->allocateRegister();
                  generateRegRegInstruction(XOR4RegReg, child, tmp, tmp, cg());
                  generateMemImmInstruction(CMP4MemImms, child, generateX86MemoryReference(child->getRegister(), 0, cg()), 0, cg());
                  generateRegRegInstruction(CMOVNE4RegReg, child, tmp, child->getRegister(), cg());
                  generateMemRegInstruction(S4MemReg, callNode, slot, tmp, cg());
                  cg()->stopUsingRegister(tmp);
                  }
               }
            else
               {
               generateMemRegInstruction(S4MemReg, callNode, slot, param, cg());
               }
            break;
         case TR::Int64:
            generateMemRegInstruction(S4MemReg, callNode, slot, param->getLowOrder(), cg());
            {
            auto highslot = generateX86MemoryReference(cSP, 0, cg());
            paramsSlotsOnStack.push(highslot);
            generateMemRegInstruction(S4MemReg, callNode, highslot, param->getHighOrder(), cg());
            }
            break;
         case TR::Float:
            generateMemRegInstruction(MOVSSMemReg, callNode, slot, param, cg());
            break;
         case TR::Double:
            paramsSlotsOnStack.push(NULL);
            generateMemRegInstruction(MOVSDMemReg, callNode, slot, param, cg());
            break;
         }
      cg()->decReferenceCount(child);
      }
   cg()->stopUsingRegister(cSP);
   int32_t argSize = paramsSlotsOnStack.size() * 4;
   int32_t adjustedArgSize = 0;
   if (argSize % 16 != 0)
      {
      adjustedArgSize = 16 - (argSize % 16);
      argSize = argSize + adjustedArgSize;
      if (comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "adjust arguments size by %d to make arguments 16 byte aligned \n", adjustedArgSize);
      }
   for(int offset = adjustedArgSize; !paramsSlotsOnStack.isEmpty(); offset += TR::Compiler->om.sizeofReferenceAddress())
      {
      auto param = paramsSlotsOnStack.pop();
      if (param)
         {
         param->getSymbolReference().setOffset(-offset-4);
         }
      }
   return argSize;
   }

#endif
