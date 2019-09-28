/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include "codegen/ARM64Instruction.hpp"
#include "codegen/ARM64PrivateLinkage.hpp"
#include "codegen/CallSnippet.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "compile/Compilation.hpp"
#include "env/StackMemoryRegion.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/ParameterSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/List.hpp"

TR::ARM64PrivateLinkage::ARM64PrivateLinkage(TR::CodeGenerator *cg)
   : TR::Linkage(cg)
   {
   int32_t i;

   _properties._properties = 0;

   _properties._registerFlags[TR::RealRegister::NoReg] = 0;
   _properties._registerFlags[TR::RealRegister::x0]    = IntegerArgument|IntegerReturn;
   _properties._registerFlags[TR::RealRegister::x1]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x2]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x3]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x4]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x5]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x6]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x7]    = IntegerArgument;

   for (i = TR::RealRegister::x8; i <= TR::RealRegister::x15; i++)
      _properties._registerFlags[i] = 0; // x8 - x15 volatile

   _properties._registerFlags[TR::RealRegister::x16]   = ARM64_Reserved; // IP0
   _properties._registerFlags[TR::RealRegister::x17]   = ARM64_Reserved; // IP1

   _properties._registerFlags[TR::RealRegister::x18]   = Preserved;

   _properties._registerFlags[TR::RealRegister::x19]   = Preserved|ARM64_Reserved; // vmThread
   _properties._registerFlags[TR::RealRegister::x20]   = Preserved|ARM64_Reserved; // Java SP

   for (i = TR::RealRegister::x21; i <= TR::RealRegister::x28; i++)
      _properties._registerFlags[i] = Preserved; // x18 - x28 Preserved

   _properties._registerFlags[TR::RealRegister::x29]   = ARM64_Reserved; // FP
   _properties._registerFlags[TR::RealRegister::lr]    = ARM64_Reserved; // LR
   _properties._registerFlags[TR::RealRegister::sp]    = ARM64_Reserved;
   _properties._registerFlags[TR::RealRegister::xzr]   = ARM64_Reserved;

   _properties._registerFlags[TR::RealRegister::v0]    = FloatArgument|FloatReturn;
   _properties._registerFlags[TR::RealRegister::v1]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v2]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v3]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v4]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v5]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v6]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v7]    = FloatArgument;

   for (i = TR::RealRegister::v8; i <= TR::RealRegister::LastFPR; i++)
      _properties._registerFlags[i] = 0; // v8 - v31 volatile

   _properties._numIntegerArgumentRegisters  = 8;
   _properties._firstIntegerArgumentRegister = 0;
   _properties._numFloatArgumentRegisters    = 8;
   _properties._firstFloatArgumentRegister   = 8;

   _properties._argumentRegisters[0]  = TR::RealRegister::x0;
   _properties._argumentRegisters[1]  = TR::RealRegister::x1;
   _properties._argumentRegisters[2]  = TR::RealRegister::x2;
   _properties._argumentRegisters[3]  = TR::RealRegister::x3;
   _properties._argumentRegisters[4]  = TR::RealRegister::x4;
   _properties._argumentRegisters[5]  = TR::RealRegister::x5;
   _properties._argumentRegisters[6]  = TR::RealRegister::x6;
   _properties._argumentRegisters[7]  = TR::RealRegister::x7;
   _properties._argumentRegisters[8]  = TR::RealRegister::v0;
   _properties._argumentRegisters[9]  = TR::RealRegister::v1;
   _properties._argumentRegisters[10] = TR::RealRegister::v2;
   _properties._argumentRegisters[11] = TR::RealRegister::v3;
   _properties._argumentRegisters[12] = TR::RealRegister::v4;
   _properties._argumentRegisters[13] = TR::RealRegister::v5;
   _properties._argumentRegisters[14] = TR::RealRegister::v6;
   _properties._argumentRegisters[15] = TR::RealRegister::v7;

   _properties._firstIntegerReturnRegister = 0;
   _properties._firstFloatReturnRegister   = 1;

   _properties._returnRegisters[0]  = TR::RealRegister::x0;
   _properties._returnRegisters[1]  = TR::RealRegister::v0;

   _properties._numAllocatableIntegerRegisters = 25;
   _properties._numAllocatableFloatRegisters   = 32;

   _properties._preservedRegisterMapForGC   = 0x1fe40000;
   _properties._methodMetaDataRegister      = TR::RealRegister::x19;
   _properties._stackPointerRegister        = TR::RealRegister::x20;
   _properties._framePointerRegister        = TR::RealRegister::x29;
   _properties._vtableIndexArgumentRegister = TR::RealRegister::x9;
   _properties._j9methodArgumentRegister    = TR::RealRegister::x0;

   // Volatile GPR (0-15) + FPR (0-31) + VFT Reg
   _properties._numberOfDependencyGPRegisters = 16 + 32 + 1;
   _properties._offsetToFirstParm             = 0; // To be determined
   _properties._offsetToFirstLocal            = 0; // To be determined
   }

TR::ARM64LinkageProperties& TR::ARM64PrivateLinkage::getProperties()
   {
   return _properties;
   }

uint32_t TR::ARM64PrivateLinkage::getRightToLeft()
   {
   return getProperties().getRightToLeft();
   }

void TR::ARM64PrivateLinkage::mapStack(TR::ResolvedMethodSymbol *method)
   {
   TR_UNIMPLEMENTED();
   }

void TR::ARM64PrivateLinkage::mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex)
   {
   TR_UNIMPLEMENTED();
   }

static void lockRegister(TR::RealRegister *regToAssign)
   {
   regToAssign->setState(TR::RealRegister::Locked);
   regToAssign->setAssignedRegister(regToAssign);
   }

void TR::ARM64PrivateLinkage::initARM64RealRegisterLinkage()
   {
   TR::Machine *machine = cg()->machine();
   TR::RealRegister *reg;
   int icount;

   reg = machine->getRealRegister(TR::RealRegister::RegNum::x16); // IP0
   lockRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::x17); // IP1
   lockRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::x19); // vmThread
   lockRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::x20); // Java SP
   lockRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::x29); // FP
   lockRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::lr); // LR
   lockRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::sp); // SP
   lockRegister(reg);

   // assign "maximum" weight to registers x0-x15
   for (icount = TR::RealRegister::x0; icount <= TR::RealRegister::x15; icount++)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000);

   // assign "maximum" weight to registers x18 and x21-x28
   machine->getRealRegister(TR::RealRegister::RegNum::x18)->setWeight(0xf000);
   for (icount = TR::RealRegister::x20; icount <= TR::RealRegister::x28; icount++)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000);

   // assign "maximum" weight to registers v0-v31
   for (icount = TR::RealRegister::v0; icount <= TR::RealRegister::v31; icount++)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000);
   }


void
TR::ARM64PrivateLinkage::setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method)
   {
   ListIterator<TR::ParameterSymbol> paramIterator(&(method->getParameterList()));
   TR::ParameterSymbol *paramCursor = paramIterator.getFirst();
   int32_t numIntArgs = 0, numFloatArgs = 0;
   const TR::ARM64LinkageProperties& properties = getProperties();

   while ( (paramCursor!=NULL) &&
           ( (numIntArgs < properties.getNumIntArgRegs()) ||
             (numFloatArgs < properties.getNumFloatArgRegs()) ) )
      {
      int32_t index = -1;

      switch (paramCursor->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Int64:
         case TR::Address:
            if (numIntArgs < properties.getNumIntArgRegs())
               {
               index = numIntArgs;
               }
            numIntArgs++;
            break;

         case TR::Float:
         case TR::Double:
            if (numFloatArgs < properties.getNumFloatArgRegs())
               {
               index = numFloatArgs;
               }
            numFloatArgs++;
            break;
         }

      paramCursor->setLinkageRegisterIndex(index);
      paramCursor = paramIterator.getNext();
      }
   }


void TR::ARM64PrivateLinkage::createPrologue(TR::Instruction *cursor)
   {
   TR_UNIMPLEMENTED();
   }

void TR::ARM64PrivateLinkage::createEpilogue(TR::Instruction *cursor)
   {
   const TR::ARM64LinkageProperties& properties = getProperties();
   TR::Machine *machine = cg()->machine();
   TR::Node *lastNode = cursor->getNode();
   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   TR::RealRegister *javaSP = machine->getRealRegister(properties.getStackPointerRegister()); // x20

   // restore preserved GPRs
   int32_t preservedRegisterOffset = cg()->getLargestOutgoingArgSize() + properties.getOffsetToFirstParm(); // outgoingArgsSize
   TR::RealRegister::RegNum firstPreservedGPR = TR::RealRegister::x28;
   TR::RealRegister::RegNum lastPreservedGPR = TR::RealRegister::x21;
   for (TR::RealRegister::RegNum r = firstPreservedGPR; r >= lastPreservedGPR; r = (TR::RealRegister::RegNum)((uint32_t)r-1))
      {
      TR::RealRegister *rr = machine->getRealRegister(r);
      if (rr->getHasBeenAssignedInMethod())
         {
         TR::MemoryReference *preservedRegMR = new (trHeapMemory()) TR::MemoryReference(javaSP, preservedRegisterOffset, cg());
         cursor = generateTrg1MemInstruction(cg(), TR::InstOpCode::ldrimmx, lastNode, rr, preservedRegMR, cursor);
         preservedRegisterOffset += 8;
         }
      }

   // remove space for preserved registers
   uint32_t frameSize = cg()->getFrameSizeInBytes();
   if (constantIsUnsignedImm12(frameSize))
      {
      cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::addimmx, lastNode, javaSP, javaSP, frameSize, cursor);
      }
   else
      {
      TR::RealRegister *x9Reg = machine->getRealRegister(TR::RealRegister::RegNum::x9);
      cursor = loadConstant32(cg(), lastNode, frameSize, x9Reg, cursor);
      cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::addx, lastNode, javaSP, javaSP, x9Reg, cursor);
      }

   // restore return address
   TR::RealRegister *lr = machine->getRealRegister(TR::RealRegister::lr);
   if (machine->getLinkRegisterKilled())
      {
      TR::MemoryReference *returnAddressMR = new (cg()->trHeapMemory()) TR::MemoryReference(javaSP, 0, cg());
      cursor = generateTrg1MemInstruction(cg(), TR::InstOpCode::ldrimmx, lastNode, lr, returnAddressMR, cursor);
      }

   // return
   generateRegBranchInstruction(cg(), TR::InstOpCode::ret, lastNode, lr, cursor);
   }

int32_t TR::ARM64PrivateLinkage::buildArgs(TR::Node *callNode,
   TR::RegisterDependencyConditions *dependencies)
   {
   return buildPrivateLinkageArgs(callNode, dependencies, TR_Private);
   }

int32_t TR::ARM64PrivateLinkage::buildPrivateLinkageArgs(TR::Node *callNode,
   TR::RegisterDependencyConditions *dependencies,
   TR_LinkageConventions linkage)
   {
   TR_ASSERT(linkage == TR_Private || linkage == TR_Helper || linkage == TR_CHelper, "Unexpected linkage convention");

   const TR::ARM64LinkageProperties& properties = getProperties();
   TR::ARM64MemoryArgument *pushToMemory = NULL;
   TR::Register *argMemReg;
   TR::Register *tempReg;
   int32_t argIndex = 0;
   int32_t numMemArgs = 0;
   int32_t from, to, step;
   int32_t totalSize = 0;

   uint32_t numIntegerArgs = 0;
   uint32_t numFloatArgs = 0;

   TR::Node *child;
   TR::DataType childType;
   TR::DataType resType = callNode->getType();

   uint32_t firstArgumentChild = callNode->getFirstArgumentIndex();

   TR::MethodSymbol *callSymbol = callNode->getSymbol()->castToMethodSymbol();

   bool isHelperCall = linkage == TR_Helper || linkage == TR_CHelper;
   bool rightToLeft = isHelperCall &&
                      //we want the arguments for induceOSR to be passed from left to right as in any other non-helper call
                      !callNode->getSymbolReference()->isOSRInductionHelper();

   if (rightToLeft)
      {
      from = callNode->getNumChildren() - 1;
      to   = firstArgumentChild;
      step = -1;
      }
   else
      {
      from = firstArgumentChild;
      to   = callNode->getNumChildren() - 1;
      step = 1;
      }

   uint32_t numIntArgRegs = properties.getNumIntArgRegs();
   uint32_t numFloatArgRegs = properties.getNumFloatArgRegs();

   TR::RealRegister::RegNum specialArgReg = TR::RealRegister::NoReg;
   switch (callSymbol->getMandatoryRecognizedMethod())
      {
      // Node: special long args are still only passed in one GPR
      case TR::java_lang_invoke_ComputedCalls_dispatchJ9Method:
         specialArgReg = getProperties().getJ9MethodArgumentRegister();
         // Other args go in memory
         numIntArgRegs   = 0;
         numFloatArgRegs = 0;
         break;
      case TR::java_lang_invoke_ComputedCalls_dispatchVirtual:
         specialArgReg = getProperties().getVTableIndexArgumentRegister();
         break;
      case TR::java_lang_invoke_MethodHandle_invokeWithArgumentsHelper:
         numIntArgRegs   = 0;
         numFloatArgRegs = 0;
         break;
      }
   if (specialArgReg != TR::RealRegister::NoReg)
      {
      // ToDo: Implement this path (Eclipse OpenJ9 Issue #7023)
      comp()->failCompilation<TR::AssertionFailure>("ComputedCall");
      }

   for (int32_t i = from; (rightToLeft && i >= to) || (!rightToLeft && i <= to); i += step)
      {
      child = callNode->getChild(i);
      childType = child->getDataType();
      int32_t multiplier;

      switch (childType)
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Int64:
         case TR::Address:
            if (numIntegerArgs >= numIntArgRegs)
               numMemArgs++;
            numIntegerArgs++;
            multiplier = (childType == TR::Int64) ? 2 : 1;
            totalSize += TR::Compiler->om.sizeofReferenceAddress() * multiplier;
            break;
         case TR::Float:
         case TR::Double:
            if (numFloatArgs >= numFloatArgRegs)
               numMemArgs++;
            numFloatArgs++;
            multiplier = (childType == TR::Double) ? 2 : 1;
            totalSize += TR::Compiler->om.sizeofReferenceAddress() * multiplier;
            break;
         default:
            TR_ASSERT(false, "Argument type %s is not supported\n", childType.toString());
         }
      }

   // From here, down, any new stack allocations will expire / die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   if (numMemArgs > 0)
      {
      pushToMemory = new (trStackMemory()) TR::ARM64MemoryArgument[numMemArgs];

      argMemReg = cg()->allocateRegister();
      }

   numIntegerArgs = 0;
   numFloatArgs = 0;

   // Helper linkage preserves all argument registers except the return register
   // TODO: C helper linkage does not, this code needs to make sure argument registers are killed in post dependencies
   for (int32_t i = from; (rightToLeft && i >= to) || (!rightToLeft && i <= to); i += step)
      {
      TR::MemoryReference *mref = NULL;
      TR::Register *argRegister;
      TR::InstOpCode::Mnemonic op;

      child = callNode->getChild(i);
      childType = child->getDataType();

      switch (childType)
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Int64:
         case TR::Address: // have to do something for GC maps here
            if (childType == TR::Address)
               {
               argRegister = pushAddressArg(child);
               }
            else if (childType == TR::Int64)
               {
               argRegister = pushLongArg(child);
               }
            else
               {
               argRegister = pushIntegerWordArg(child);
               }
            if (numIntegerArgs < numIntArgRegs)
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  if (argRegister->containsCollectedReference())
                     tempReg = cg()->allocateCollectedReferenceRegister();
                  else
                     tempReg = cg()->allocateRegister();
                  generateMovInstruction(cg(), callNode, tempReg, argRegister);
                  argRegister = tempReg;
                  }
               TR::addDependency(dependencies, argRegister, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
               }
            else // numIntegerArgs >= numIntArgRegs
               {
               op = ((childType == TR::Address) || (childType == TR::Int64)) ? TR::InstOpCode::strpostx : TR::InstOpCode::strpostw;
               mref = getOutgoingArgumentMemRef(argMemReg, argRegister, op, pushToMemory[argIndex++]);
               }
            numIntegerArgs++;
            break;
         case TR::Float:
         case TR::Double:
            if (childType == TR::Float)
               {
               argRegister = pushFloatArg(child);
               }
            else
               {
               argRegister = pushDoubleArg(child);
               }
            if (numFloatArgs < numFloatArgRegs)
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  tempReg = cg()->allocateRegister(TR_FPR);
                  op = (childType == TR::Float) ? TR::InstOpCode::fmovs : TR::InstOpCode::fmovd;
                  generateTrg1Src1Instruction(cg(), op, callNode, tempReg, argRegister);
                  argRegister = tempReg;
                  }
               if (numFloatArgs == 0 && resType.isFloatingPoint())
                  {
                  TR::Register *resultReg;
                  if (resType.getDataType() == TR::Float)
                     resultReg = cg()->allocateSinglePrecisionRegister();
                  else
                     resultReg = cg()->allocateRegister(TR_FPR);
                  dependencies->addPreCondition(argRegister, TR::RealRegister::v0);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::v0);
                  }
               else
                  TR::addDependency(dependencies, argRegister, properties.getFloatArgumentRegister(numFloatArgs), TR_FPR, cg());
               }
            else // numFloatArgs >= numFloatArgRegs
               {
               op = (childType == TR::Float) ? TR::InstOpCode::vstrposts : TR::InstOpCode::vstrpostd;
               mref = getOutgoingArgumentMemRef(argMemReg, argRegister, op, pushToMemory[argIndex++]);
               }
            numFloatArgs++;
            break;
         }
      }

   for (int32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastGPR; ++i)
      {
      TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)i;
      if (properties.getPreserved(realReg) || (properties.getRegisterFlags(realReg) & ARM64_Reserved))
         continue;
      if (realReg == specialArgReg)
         continue; // already added deps above.  No need to add them here.
      if (!dependencies->searchPreConditionRegister(realReg))
         {
         if (realReg == properties.getIntegerArgumentRegister(0) && callNode->getDataType() == TR::Address)
            {
            dependencies->addPreCondition(cg()->allocateRegister(), TR::RealRegister::x0);
            dependencies->addPostCondition(cg()->allocateCollectedReferenceRegister(), TR::RealRegister::x0);
            }
         else
            {
            // Helper linkage preserves all registers that are not argument registers, so we don't need to spill them.
            if (linkage != TR_Helper)
               TR::addDependency(dependencies, NULL, realReg, TR_GPR, cg());
            }
         }
      }

   if (callNode->getType().isFloatingPoint() && numFloatArgs == 0)
      {
      //add return floating-point register dependency
      TR::addDependency(dependencies, NULL, (TR::RealRegister::RegNum)getProperties().getFloatReturnRegister(), TR_FPR, cg());
      }

   for (int32_t i = TR::RealRegister::FirstFPR; i <= TR::RealRegister::LastFPR; ++i)
      {
      TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)i;
      if (properties.getPreserved(realReg))
         continue;
      if (!dependencies->searchPreConditionRegister(realReg))
         {
         TR::addDependency(dependencies, NULL, realReg, TR_FPR, cg());
         }
      }

   if (numMemArgs > 0)
      {
      TR::RealRegister *sp = cg()->machine()->getRealRegister(properties.getStackPointerRegister());
      generateMovInstruction(cg(), callNode, argMemReg, sp);

      for (argIndex = 0; argIndex < numMemArgs; argIndex++)
         {
         TR::Register *aReg = pushToMemory[argIndex].argRegister;
         generateMemSrc1Instruction(cg(), pushToMemory[argIndex].opCode, callNode, pushToMemory[argIndex].argMemory, aReg);
         cg()->stopUsingRegister(aReg);
         }

      cg()->stopUsingRegister(argMemReg);
      }

   return totalSize;
   }

void TR::ARM64PrivateLinkage::buildDirectCall(TR::Node *callNode,
   TR::SymbolReference *callSymRef,
   TR::RegisterDependencyConditions *dependencies,
   const TR::ARM64LinkageProperties &pp,
   uint32_t argSize)
   {
   TR::Instruction *gcPoint;
   TR::MethodSymbol *callSymbol = callSymRef->getSymbol()->castToMethodSymbol();

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());

   if (callSymRef->getReferenceNumber() >= TR_ARM64numRuntimeHelpers)
      fej9->reserveTrampolineIfNecessary(comp(), callSymRef, false);

   bool forceUnresolvedDispatch = fej9->forceUnresolvedDispatch();

   if (callSymbol->isJITInternalNative() ||
       (!callSymRef->isUnresolved() && !callSymbol->isInterpreted() &&
        ((forceUnresolvedDispatch && callSymbol->isHelper()) || !forceUnresolvedDispatch)))
      {
      bool isMyself = comp()->isRecursiveMethodTarget(callSymbol);

      gcPoint = generateImmSymInstruction(cg(), TR::InstOpCode::bl, callNode,
         isMyself ? 0 : (uintptr_t)callSymbol->getMethodAddress(),
         dependencies,
         callSymRef ? callSymRef : callNode->getSymbolReference(),
         NULL);
      }
   else
      {
      TR::LabelSymbol *label = generateLabelSymbol(cg());
      TR::Snippet *snippet;

      if (callSymRef->isUnresolved())
         {
         snippet = new (trHeapMemory()) TR::ARM64UnresolvedCallSnippet(cg(), callNode, label, argSize);
         }
      else
         {
         snippet = new (trHeapMemory()) TR::ARM64CallSnippet(cg(), callNode, label, argSize);
         snippet->gcMap().setGCRegisterMask(pp.getPreservedRegisterMapForGC());
         }

      cg()->addSnippet(snippet);
      gcPoint = generateImmSymInstruction(cg(), TR::InstOpCode::bl, callNode,
         0, dependencies,
         new (trHeapMemory()) TR::SymbolReference(comp()->getSymRefTab(), label),
         snippet);
      }

   gcPoint->ARM64NeedsGCMap(cg(), callSymbol->getLinkageConvention() == TR_Helper ? 0xffffffff : pp.getPreservedRegisterMapForGC());
   }

TR::Register *TR::ARM64PrivateLinkage::buildDirectDispatch(TR::Node *callNode)
   {
   TR::SymbolReference *callSymRef = callNode->getSymbolReference();
   const TR::ARM64LinkageProperties &pp = getProperties();
   TR::RegisterDependencyConditions *dependencies =
      new (trHeapMemory()) TR::RegisterDependencyConditions(
         pp.getNumberOfDependencyGPRegisters(),
         pp.getNumberOfDependencyGPRegisters(), trMemory());

   int32_t argSize = buildArgs(callNode, dependencies);

   buildDirectCall(callNode, callSymRef, dependencies, pp, argSize);
   cg()->machine()->setLinkRegisterKilled(true);

   TR::Register *retReg;
   switch(callNode->getOpCodeValue())
      {
      case TR::icall:
         retReg = dependencies->searchPostConditionRegister(
                     pp.getIntegerReturnRegister());
         break;
      case TR::lcall:
      case TR::acall:
         retReg = dependencies->searchPostConditionRegister(
                     pp.getLongReturnRegister());
         break;
      case TR::fcall:
      case TR::dcall:
         retReg = dependencies->searchPostConditionRegister(
                     pp.getFloatReturnRegister());
         break;
      case TR::call:
         retReg = NULL;
         break;
      default:
         retReg = NULL;
         TR_ASSERT_FATAL(false, "Unsupported direct call Opcode.");
      }

   callNode->setRegister(retReg);
   return retReg;
   }

static TR::Register *evaluateUpToVftChild(TR::Node *callNode, TR::CodeGenerator *cg)
   {
   TR::Register *vftReg = NULL;
   if (callNode->getFirstArgumentIndex() == 1)
      {
      TR::Node *child = callNode->getFirstChild();
      vftReg = cg->evaluate(child);
      cg->decReferenceCount(child);
      }
   TR_ASSERT_FATAL(vftReg != NULL, "Failed to find vft child.");
   return vftReg;
   }

void TR::ARM64PrivateLinkage::buildVirtualDispatch(TR::Node *callNode,
   TR::RegisterDependencyConditions *dependencies,
   uint32_t argSize)
   {
   TR::Register *x0 = dependencies->searchPreConditionRegister(TR::RealRegister::x0);
   TR::Register *x9 = dependencies->searchPreConditionRegister(TR::RealRegister::x9);

   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   TR::LabelSymbol *doneLabel = NULL;

   TR::Instruction *gcPoint;

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());

   // Computed calls
   //
   if (methodSymbol->isComputed())
      {
      // ToDo: Implement this path (Eclipse OpenJ9 Issue #7023)
      comp()->failCompilation<TR::AssertionFailure>("ComputedCall");
      }

   // Virtual and interface calls
   //
   TR_ASSERT_FATAL(methodSymbol->isVirtual() || methodSymbol->isInterface(), "Unexpected method type");

   void *thunk = fej9->getJ2IThunk(methodSymbol->getMethod(), comp());
   if (!thunk)
      thunk = fej9->setJ2IThunk(methodSymbol->getMethod(), TR::ARM64CallSnippet::generateVIThunk(callNode, argSize, cg()), comp());

   if (methodSymbol->isVirtual())
      {
      TR::MemoryReference *tempMR;
      TR::Register *vftReg = evaluateUpToVftChild(callNode, cg());
      TR::addDependency(dependencies, vftReg, TR::RealRegister::NoReg, TR_GPR, cg());

      if (methodSymRef->isUnresolved() || comp()->compileRelocatableCode())
         {
         doneLabel = generateLabelSymbol(cg());

         TR::LabelSymbol *vcSnippetLabel = generateLabelSymbol(cg());
         TR::ARM64VirtualUnresolvedSnippet *vcSnippet =
            new (trHeapMemory())
            TR::ARM64VirtualUnresolvedSnippet(cg(), callNode, vcSnippetLabel, argSize, doneLabel, (uint8_t *)thunk);
         cg()->addSnippet(vcSnippet);

         TR::Register *dstReg = cg()->allocateRegister();

         // The following instructions are modified by _virtualUnresolvedHelper
         // in aarch64/runtime/PicBuilder.spp to load the vTable index in x9
         generateTrg1ImmInstruction(cg(), TR::InstOpCode::movzx, callNode, x9, 0);
         generateTrg1ImmInstruction(cg(), TR::InstOpCode::movkx, callNode, x9, TR::MOV_LSL16);
         generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::sbfmx, callNode, x9, x9, 0x1F); // sxtw x9, w9
         tempMR = new (trHeapMemory()) TR::MemoryReference(vftReg, x9, cg());
         generateTrg1MemInstruction(cg(), TR::InstOpCode::ldroffx, callNode, dstReg, tempMR);
         gcPoint = generateLabelInstruction(cg(), TR::InstOpCode::b, callNode, vcSnippetLabel, dependencies);

         cg()->stopUsingRegister(dstReg);
         }
      else
         {
         int32_t offset = methodSymRef->getOffset();
         TR_ASSERT(offset < 0, "Unexpected positive offset for virtual call");

         // jitVTableIndex() in oti/JITInterface.hpp assumes the instruction sequence below
         if (offset >= -65536)
            {
            generateTrg1ImmInstruction(cg(), TR::InstOpCode::movnx, callNode, x9, ~offset & 0xFFFF);
            }
         else
            {
            generateTrg1ImmInstruction(cg(), TR::InstOpCode::movzx, callNode, x9, offset & 0xFFFF);
            generateTrg1ImmInstruction(cg(), TR::InstOpCode::movkx, callNode, x9,
                                       (((offset >> 16) & 0xFFFF) | TR::MOV_LSL16));
            generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::sbfmx, callNode, x9, x9, 0x1F); // sxtw x9, w9
            }
         tempMR = new (trHeapMemory()) TR::MemoryReference(vftReg, x9, cg());
         generateTrg1MemInstruction(cg(), TR::InstOpCode::ldroffx, callNode, x9, tempMR);
         gcPoint = generateRegBranchInstruction(cg(), TR::InstOpCode::blr, callNode, x9, dependencies);
         }
      }
   else
      {
      // interface calls
      // ToDo: Inline interface dispatch
      doneLabel = generateLabelSymbol(cg());

      TR::LabelSymbol *ifcSnippetLabel = generateLabelSymbol(cg());
      TR::ARM64InterfaceCallSnippet *ifcSnippet =
         new (trHeapMemory())
         TR::ARM64InterfaceCallSnippet(cg(), callNode, ifcSnippetLabel, argSize, doneLabel, (uint8_t *)thunk);
      cg()->addSnippet(ifcSnippet);

      gcPoint = generateLabelInstruction(cg(), TR::InstOpCode::b, callNode, ifcSnippetLabel, dependencies);
      }

   gcPoint->ARM64NeedsGCMap(cg(), getProperties().getPreservedRegisterMapForGC());

   if (doneLabel)
      generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, doneLabel, dependencies);

   return;
   }

TR::Register *TR::ARM64PrivateLinkage::buildIndirectDispatch(TR::Node *callNode)
   {
   const TR::ARM64LinkageProperties &pp = getProperties();
   TR::RealRegister *sp = cg()->machine()->getRealRegister(pp.getStackPointerRegister());

   TR::RegisterDependencyConditions *dependencies =
      new (trHeapMemory()) TR::RegisterDependencyConditions(
         pp.getNumberOfDependencyGPRegisters(),
         pp.getNumberOfDependencyGPRegisters(), trMemory());

   int32_t argSize = buildArgs(callNode, dependencies);

   buildVirtualDispatch(callNode, dependencies, argSize);
   cg()->machine()->setLinkRegisterKilled(true);

   TR::Register *retReg;
   switch(callNode->getOpCodeValue())
      {
      case TR::icalli:
         retReg = dependencies->searchPostConditionRegister(
                     pp.getIntegerReturnRegister());
         break;
      case TR::lcalli:
      case TR::acalli:
         retReg = dependencies->searchPostConditionRegister(
                     pp.getLongReturnRegister());
         break;
      case TR::fcalli:
      case TR::dcalli:
         retReg = dependencies->searchPostConditionRegister(
                     pp.getFloatReturnRegister());
         break;
      case TR::calli:
         retReg = NULL;
         break;
      default:
         retReg = NULL;
         TR_ASSERT_FATAL(false, "Unsupported indirect call Opcode.");
      }

   callNode->setRegister(retReg);
   return retReg;
   }

int32_t TR::ARM64HelperLinkage::buildArgs(TR::Node *callNode,
   TR::RegisterDependencyConditions *dependencies)
   {
   return buildPrivateLinkageArgs(callNode, dependencies, _helperLinkage);
   }

TR::Instruction *
TR::ARM64PrivateLinkage::loadStackParametersToLinkageRegisters(TR::Instruction *cursor)
   {
   TR::Machine *machine = cg()->machine();
   TR::ARM64LinkageProperties& properties = getProperties();
   TR::RealRegister *javaSP = machine->getRealRegister(properties.getStackPointerRegister());       // x20

   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   ListIterator<TR::ParameterSymbol> parmIterator(&(bodySymbol->getParameterList()));
   TR::ParameterSymbol *parmCursor;

   // Copy from stack all parameters that belong in linkage regs
   //
   for (parmCursor = parmIterator.getFirst();
        parmCursor != NULL;
        parmCursor = parmIterator.getNext())
      {
      if (parmCursor->isParmPassedInRegister())
         {
         int8_t lri = parmCursor->getLinkageRegisterIndex();
         TR::RealRegister *linkageReg;
         TR::InstOpCode::Mnemonic op;

         if (parmCursor->getDataType() == TR::Double || parmCursor->getDataType() == TR::Float)
            {
            linkageReg = machine->getRealRegister(properties.getFloatArgumentRegister(lri));
            op = (parmCursor->getDataType() == TR::Double) ? TR::InstOpCode::vldrimmd : TR::InstOpCode::vldrimms;
            }
         else
            {
            linkageReg = machine->getRealRegister(properties.getIntegerArgumentRegister(lri));
            op = TR::InstOpCode::ldrimmx;
            }

         TR::MemoryReference *stackMR = new (cg()->trHeapMemory()) TR::MemoryReference(javaSP, parmCursor->getParameterOffset(), cg());
         cursor = generateTrg1MemInstruction(cg(), op, NULL, linkageReg, stackMR, cursor);
         }
      }

    return cursor;
    }

/**
 * A more optimal implementation of this function will be required when GRA is enabled.
 * See OpenJ9 issue #6657.  Also consider merging with loadStackParametersToLinkageRegisters.
 */
TR::Instruction *
TR::ARM64PrivateLinkage::copyParametersToHomeLocation(TR::Instruction *cursor)
   {
   TR::Machine *machine = cg()->machine();
   TR::ARM64LinkageProperties& properties = getProperties();
   TR::RealRegister *javaSP = machine->getRealRegister(properties.getStackPointerRegister());       // x20

   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   ListIterator<TR::ParameterSymbol> parmIterator(&(bodySymbol->getParameterList()));
   TR::ParameterSymbol *parmCursor;

   // Store to stack all parameters passed in linkage registers
   //
   for (parmCursor = parmIterator.getFirst();
        parmCursor != NULL;
        parmCursor = parmIterator.getNext())
      {
      if (parmCursor->isParmPassedInRegister())
         {
         int8_t lri = parmCursor->getLinkageRegisterIndex();
         TR::RealRegister *linkageReg;
         TR::InstOpCode::Mnemonic op;

         if (parmCursor->getDataType() == TR::Double || parmCursor->getDataType() == TR::Float)
            {
            linkageReg = machine->getRealRegister(properties.getFloatArgumentRegister(lri));
            op = (parmCursor->getDataType() == TR::Double) ? TR::InstOpCode::vstrimmd : TR::InstOpCode::vstrimms;
            }
         else
            {
            linkageReg = machine->getRealRegister(properties.getIntegerArgumentRegister(lri));
            op = TR::InstOpCode::strimmx;
            }

         TR::MemoryReference *stackMR = new (cg()->trHeapMemory()) TR::MemoryReference(javaSP, parmCursor->getParameterOffset(), cg());
         cursor = generateMemSrc1Instruction(cg(), op, NULL, stackMR, linkageReg, cursor);
         }
      }

   return cursor;
   }

