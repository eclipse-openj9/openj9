/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#if defined(TR_TARGET_32BIT)

#include "codegen/IA32J9SystemLinkage.hpp"
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

   // For java, we generate VFPDedicate instruction to dedicate ebx as vfp
   TR::Register *ebxReal = cg()->allocateRegister();
   deps->addPostCondition(ebxReal, TR::RealRegister::ebx, cg());

   deps->addPostCondition(cg()->getMethodMetaDataRegister(), TR::RealRegister::ebp, cg());

   if (cg()->useSSEForSinglePrecision() || cg()->useSSEForDoublePrecision())
      {
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

   cg()->setVMThreadRequired(true);

   TR::RealRegister    *stackPointerReg = machine()->getX86RealRegister(TR::RealRegister::esp);
   TR::SymbolReference *methodSymRef    = callNode->getSymbolReference();
   TR::MethodSymbol    *methodSymbol    = callNode->getSymbol()->castToMethodSymbol();
   uint8_t numXmmRegs = 0;

   if (!methodSymbol->isHelper())
      diagnostic("Building call site for %s\n", methodSymbol->getMethod()->signature(trMemory()));

   TR::X86VFPDedicateInstruction  *vfpDedicateInstruction = generateVFPDedicateInstruction(machine()->getX86RealRegister(TR::RealRegister::ebx), callNode, cg());

   cg()->setVMThreadRequired(true);

   TR::J9LinkageUtils::switchToMachineCStack(callNode, cg());

   // check to see if we need to set XMM register dependencies
   if (cg()->useSSEForSinglePrecision() || cg()->useSSEForDoublePrecision())
      {
      TR_LiveRegisters *lr = cg()->getLiveRegisters(TR_FPR);
      if (!lr || lr->getNumberOfLiveRegisters() > 0)
         numXmmRegs = (uint8_t)(TR::RealRegister::LastXMMR - TR::RealRegister::FirstXMMR+1);
      }

   TR::RegisterDependencyConditions  *deps;
   deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)6 + numXmmRegs, cg());
   TR::Register *returnReg = buildVolatileAndReturnDependencies(callNode, deps);

   TR::RegisterDependencyConditions  *dummy = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)0, cg());

   uint32_t  argSize = buildArgs(callNode, dummy);

   TR::Register* targetAddressReg = NULL;
   TR::MemoryReference* targetAddressMem = NULL;

   // Force the FP register stack to be spilled.
   //
   if (!cg()->useSSEForDoublePrecision())
      {
      TR::RegisterDependencyConditions  *fpSpillDependency = generateRegisterDependencyConditions(1, 0, cg());
      fpSpillDependency->addPreCondition(NULL, TR::RealRegister::AllFPRegisters, cg());
      generateInstruction(FPREGSPILL, callNode, fpSpillDependency, cg());
      }


   // Call-out
   int32_t stackAdjustment = -argSize;
   TR::X86ImmInstruction* instr = generateImmSymInstruction(CALLImm4, callNode, (uintptr_t)methodSymbol->getMethodAddress(), methodSymRef, cg());
   instr->setAdjustsFramePointerBy(stackAdjustment);

   // Explicitly change esp to be current esp + argSize
   //
   generateRegImmInstruction(
      ADD4RegImms,
      callNode,
      stackPointerReg,
      argSize,
      cg()
      );

   if (returnReg && !(methodSymbol->isHelper()))
      TR::J9LinkageUtils::cleanupReturnValue(callNode, returnReg, returnReg, cg());

   TR::J9LinkageUtils::switchToJavaStack(callNode, cg());

   generateVFPReleaseInstruction(vfpDedicateInstruction, callNode, cg());
   cg()->setVMThreadRequired(false);

   // Label denoting end of dispatch code sequence; dependencies are on
   // this label rather than on the call
   //
   TR::LabelSymbol *endSystemCallSequence = generateLabelSymbol(cg());
   generateLabelInstruction(LABEL, callNode, endSystemCallSequence, deps, cg());

   // Stop using the killed registers that are not going to persist
   //
   if (deps)
      stopUsingKilledRegisters(deps, returnReg);

   if (cg()->useSSEForSinglePrecision() && callNode->getOpCode().isFloat())
      {
   TR::MemoryReference  *tempMR = machine()->getDummyLocalMR(TR::Float);
      generateFPMemRegInstruction(FSTPMemReg, callNode, tempMR, returnReg, cg());
      cg()->stopUsingRegister(returnReg); // zhxingl: this is added by me
      returnReg = cg()->allocateSinglePrecisionRegister(TR_FPR);
      generateRegMemInstruction(MOVSSRegMem, callNode, returnReg, generateX86MemoryReference(*tempMR, 0, cg()), cg());
      }
   else if (cg()->useSSEForDoublePrecision() && callNode->getOpCode().isDouble())
      {
   TR::MemoryReference  *tempMR = machine()->getDummyLocalMR(TR::Double);
      generateFPMemRegInstruction(DSTPMemReg, callNode, tempMR, returnReg, cg());
      cg()->stopUsingRegister(returnReg); // zhxingl: this is added by me
      returnReg = cg()->allocateRegister(TR_FPR);
      generateRegMemInstruction(cg()->getXMMDoubleLoadOpCode(), callNode, returnReg, generateX86MemoryReference(*tempMR, 0, cg()), cg());
      }
   else
      {
      if ((callNode->getDataType() == TR::Float ||
           callNode->getDataType() == TR::Double) &&
          callNode->getReferenceCount() == 1)
         {
         generateFPSTiST0RegRegInstruction(FSTRegReg, callNode, returnReg, returnReg, cg());
         }
      }

   if (cg()->enableRegisterAssociations())
      associatePreservedRegisters(deps, returnReg);

   cg()->setVMThreadRequired(false);

   return returnReg;
   }

int32_t
TR::IA32J9SystemLinkage::buildArgs(
    TR::Node *callNode,
    TR::RegisterDependencyConditions *deps)
   {
   // Push args in reverse order for a system call
   //
   int32_t argSize = 0;
   int32_t firstArg = callNode->getFirstArgumentIndex();
   for (int i = callNode->getNumChildren() - 1; i >= firstArg; i--)
      {
      TR::Node *child = callNode->getChild(i);
      switch (child->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Address:
         case TR::Int32:
            TR::IA32LinkageUtils::pushIntegerWordArg(child,cg());
            argSize += 4;
            break;
         case TR::Float:
            TR::IA32LinkageUtils::pushFloatArg(child,cg());
            argSize += 4;
            break;
         case TR::Double:
            TR::IA32LinkageUtils::pushDoubleArg(child, cg());
            argSize += 8;
            break;
         case TR::Aggregate:
         case TR::Int64:
         default:
            TR_ASSERT(0, "Attempted to push unknown type");
            break;
         }
      }
   return argSize;
   }

#endif
