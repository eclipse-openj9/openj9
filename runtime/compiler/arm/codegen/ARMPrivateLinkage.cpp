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

#include "arm/codegen/ARMPrivateLinkage.hpp"

#include "arm/codegen/ARMInstruction.hpp"
#include "codegen/CallSnippet.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/StackCheckFailureSnippet.hpp"
#include "env/CompilerEnv.hpp"
#include "env/J2IThunk.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "env/VMJ9.h"

#define LOCK_R14
// #define DEBUG_ARM_LINKAGE

TR::ARMLinkageProperties TR::ARMPrivateLinkage::properties =
   {                           // TR_Private
    0,                         // linkage properties
       {                        // register flags
       0,                       // NoReg
       0,                       // gr0
       0,                       // gr1
       0,                       // gr2
       0,                       // gr3
       0, /*Preserved,*/        // gr4
       0, /*Preserved,*/        // gr5
       Preserved,               // gr6  Java BP
       Preserved,               // gr7  Java SP
       Preserved,               // gr8  Java J9VMThread
       Preserved,               // gr9
       Preserved,               // gr10
       0, /* Preserved,*/       // gr11 APCS FP (ignore for now - use as temp)
       Preserved,               // gr12 (OS RESERVED)
       0, /* Preserved,*/       // gr13 APCS SP (OS RESERVED)
       0,                       // gr14 LR
       0,                       // gr15 IP
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
       0,                       // f0
       0,                       // f1
       0,                       // f2
       0,                       // f3
       0,                       // f4
       0,                       // f5
       0,                       // f6
       0,                       // f7
       0,                       // f8
       0,                       // f9
       0,                       // f10
       0,                       // f11
       0,                       // f12
       0,                       // f13
       0,                       // f14
       0,                       // f15
#else
       FloatReturn,             // f0
       0,                       // f1
       0,                       // f2
       0,                       // f3
       Preserved,               // f4
       Preserved,               // f5
       Preserved,               // f6
       Preserved,               // f7
#endif
       },
       {                        // preserved registers
//       TR::RealRegister::gr4,
//       TR::RealRegister::gr5,
       TR::RealRegister::gr6,
       TR::RealRegister::gr7,
       TR::RealRegister::gr8,
       TR::RealRegister::gr9,
       TR::RealRegister::gr10,
//       TR::RealRegister::gr11,
//       TR::RealRegister::gr13,
//       TR::RealRegister::gr14,
       TR::RealRegister::gr15,
#if !defined(__VFP_FP__) || defined(__SOFTFP__)
       TR::RealRegister::fp4,
       TR::RealRegister::fp5,
       TR::RealRegister::fp6,
       TR::RealRegister::fp7
#endif
       },
       {                        // argument registers
       TR::RealRegister::gr0,
       TR::RealRegister::gr1,
       TR::RealRegister::gr2,
       TR::RealRegister::gr3,
#if !defined(__VFP_FP__) || defined(__SOFTFP__)
       TR::RealRegister::fp0,
       TR::RealRegister::fp1,
       TR::RealRegister::fp2,
       TR::RealRegister::fp3,
#endif
       },
       {                        // return registers
       TR::RealRegister::gr0,
       TR::RealRegister::gr1,
#if !defined(__VFP_FP__) || defined(__SOFTFP__)
       TR::RealRegister::fp0,
#endif
       },
       MAX_ARM_GLOBAL_GPRS,     // numAllocatableIntegerRegisters
       MAX_ARM_GLOBAL_FPRS,     // numAllocatableFloatRegisters
       0x0000FFC0,               // preserved register map
       TR::RealRegister::gr6,  // frame register
       TR::RealRegister::gr8,  // method meta data register
       TR::RealRegister::gr7,  // stack pointer register
       TR::RealRegister::gr11, // vtable index register
       TR::RealRegister::gr0,  // j9method argument register
       15,                       // numberOfDependencyRegisters
       0,                        // offsetToFirstStackParm
       -4,                       // offsetToFirstLocal
       4,                        // numIntegerArgumentRegisters
       0,                        // firstIntegerArgumentRegister
#if defined(__VFP_FP__) && !defined(__SOFTFP__)
       0,                        // numFloatArgumentRegisters
#else
       4,                        // numFloatArgumentRegisters
#endif
       0,                        // firstFloatArgumentRegister
       0,                        // firstIntegerReturnRegister
       0                         // firstFloatReturnRegister
   };

TR::ARMLinkageProperties& TR::ARMPrivateLinkage::getProperties()
   {
   return properties;
   }

static void lockRegister(TR::RealRegister *regToAssign)
   {
   regToAssign->setState(TR::RealRegister::Locked);
   regToAssign->setAssignedRegister(regToAssign);
   }

void TR::ARMPrivateLinkage::initARMRealRegisterLinkage()
   {
   TR::CodeGenerator           *codeGen     = cg();
   TR::Machine                 *machine = codeGen->machine();
   const TR::ARMLinkageProperties &linkage     = getProperties();
   int icount;

   for (icount = TR::RealRegister::FirstGPR; icount <= TR::RealRegister::gr5; icount++)
      {
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(icount);
      }

   // mark all magic registers as locked and having a physical
   // register associated so the register assigner stays happy
   lockRegister(codeGen->getMethodMetaDataRegister());
   lockRegister(codeGen->getFrameRegister());
   lockRegister(codeGen->machine()->getRealRegister(properties.getStackPointerRegister()));
   lockRegister(machine->getRealRegister(TR::RealRegister::gr12)); // r12 is OS reserved
   lockRegister(machine->getRealRegister(TR::RealRegister::gr13)); // r13 is OS SP
#ifdef LOCK_R14
   lockRegister(machine->getRealRegister(TR::RealRegister::gr14)); // r14 is LR
#endif
   lockRegister(machine->getRealRegister(TR::RealRegister::gr15)); // r15 is IP

   for (icount=TR::RealRegister::LastGPR; icount>=TR::RealRegister::gr6; icount--)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000-icount);

#if defined(__VFP_FP__) && !defined(__SOFTFP__)
   for (icount=TR::RealRegister::FirstFPR; icount<=TR::RealRegister::LastFPR; icount++)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(icount);
#else
   for (icount=TR::RealRegister::FirstFPR; icount<=TR::RealRegister::fp3; icount++)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(icount);

   for (icount=TR::RealRegister::LastFPR; icount>=TR::RealRegister::fp4; icount--)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000-icount);
#endif
   }

uint32_t TR::ARMPrivateLinkage::getRightToLeft()
   {
   return getProperties().getRightToLeft();
   }

void TR::ARMPrivateLinkage::mapStack(TR::ResolvedMethodSymbol *method)
   {
   ListIterator<TR::AutomaticSymbol>  automaticIterator(&method->getAutomaticList());
   TR::AutomaticSymbol               *localCursor       = automaticIterator.getFirst();
   const TR::ARMLinkageProperties&    linkage           = getProperties();
   TR::CodeGenerator              *codeGen           = cg();
   TR::Machine                    *machine           = codeGen->machine();
   int32_t                           firstLocalOffset  = linkage.getOffsetToFirstLocal();
   uint32_t                          stackIndex        = firstLocalOffset;
   int32_t                           lowGCOffset;
   TR::GCStackAtlas                  *atlas             = codeGen->getStackAtlas();

   // map all garbage collected references together so can concisely represent
   // stack maps. They must be mapped so that the GC map index in each local
   // symbol is honoured.

   lowGCOffset = stackIndex;
   int32_t firstLocalGCIndex = atlas->getNumberOfParmSlotsMapped();

   stackIndex -= (atlas->getNumberOfSlotsMapped() - atlas->getNumberOfParmSlotsMapped()) << 2;

   // Map local references again to set the stack position correct according to
   // the GC map index.
   //
   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      if (localCursor->getGCMapIndex() >= 0)
         {
         localCursor->setOffset(stackIndex + 4 * (localCursor->getGCMapIndex() - firstLocalGCIndex));
         if (localCursor->getGCMapIndex() == atlas->getIndexOfFirstInternalPointer())
            {
            atlas->setOffsetOfFirstInternalPointer(localCursor->getOffset() - firstLocalOffset);
            }
         }

   method->setObjectTempSlots((lowGCOffset-stackIndex) >> 2);
   lowGCOffset = stackIndex;

   // Now map the rest of the locals
   //
   automaticIterator.reset();
   localCursor = automaticIterator.getFirst();

   while (localCursor != NULL)
      {
      if (localCursor->getGCMapIndex() < 0 &&
          localCursor->getDataType() != TR::Double)
         {
         mapSingleAutomatic(localCursor, stackIndex);
         }
      localCursor = automaticIterator.getNext();
      }

   automaticIterator.reset();
   localCursor = automaticIterator.getFirst();

   while (localCursor != NULL)
      {
      if (localCursor->getDataType() == TR::Double)
         {
         stackIndex -= (stackIndex & 0x4)?4:0;
         mapSingleAutomatic(localCursor, stackIndex);
         }
      localCursor = automaticIterator.getNext();
      }
   method->setLocalMappingCursor(stackIndex);

   ListIterator<TR::ParameterSymbol> parameterIterator(&method->getParameterList());
   TR::ParameterSymbol              *parmCursor = parameterIterator.getFirst();
   int32_t                          offsetToFirstParm = linkage.getOffsetToFirstParm();
   if (linkage.getRightToLeft())
      {
      while (parmCursor != NULL)
         {
         parmCursor->setParameterOffset(parmCursor->getParameterOffset() + offsetToFirstParm);
         parmCursor = parameterIterator.getNext();
         }
      }
   else
      {
      uint32_t sizeOfParameterArea = method->getNumParameterSlots() << 2;
      while (parmCursor != NULL)
         {
         parmCursor->setParameterOffset(sizeOfParameterArea -
                                        parmCursor->getParameterOffset() -
                                        parmCursor->getSize() +
                                        offsetToFirstParm);
         parmCursor = parameterIterator.getNext();
         }
      }

   atlas->setLocalBaseOffset(lowGCOffset - firstLocalOffset);
   atlas->setParmBaseOffset(atlas->getParmBaseOffset() + offsetToFirstParm - firstLocalOffset);
   }

void TR::ARMPrivateLinkage::mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex)
   {
   int32_t roundedSize = (p->getSize()+3)&(~3);
   if (roundedSize == 0)
      roundedSize = 4;

   p->setOffset(stackIndex -= roundedSize);
   }

void TR::ARMPrivateLinkage::setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method)
   {
   ListIterator<TR::ParameterSymbol>   paramIterator(&(method->getParameterList()));
   TR::ParameterSymbol      *paramCursor = paramIterator.getFirst();
   int32_t                  numIntArgs = 0;
   const TR::ARMLinkageProperties& properties = getProperties();

   while ( (paramCursor!=NULL) &&
           (numIntArgs < properties.getNumIntArgRegs()) )
      {
      int32_t index = -1;

      switch (paramCursor->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address:
         case TR::Float://float args are passed in gpr
            if (numIntArgs<properties.getNumIntArgRegs())
               {
               index = numIntArgs;
               }
            numIntArgs++;
            break;
         case TR::Int64:
         case TR::Double://float args are passed in 2 gprs
            if (numIntArgs<properties.getNumIntArgRegs())
               {
               index = numIntArgs;
               }
            numIntArgs += 2;
            break;
         }
      paramCursor->setLinkageRegisterIndex(index);
      paramCursor = paramIterator.getNext();
      }

   }

// the following three functions are to be modified together
// all lists are dealing with the _preserved_ registers on the machine
uint16_t getAssignedRegisterList(TR::Machine *machine, const TR::ARMLinkageProperties *linkage)
   {
   uint16_t assignedRegisters = 0;

   for(int i = TR::RealRegister::LastGPR; i; i--)
      {
      assignedRegisters <<= 1;
      if(linkage->getPreserved((TR::RealRegister::RegNum)i))
         {
         assignedRegisters |= machine->getRealRegister((TR::RealRegister::RegNum)i)->getHasBeenAssignedInMethod();
         }
      }

   return assignedRegisters;
   }

uint16_t getAssignedRegisterCount(TR::Machine *machine, const TR::ARMLinkageProperties *linkage)
   {
   uint16_t assignedRegisters = 0;

   for (int i = TR::RealRegister::LastGPR; i; i--)
      {
      if (linkage->getPreserved((TR::RealRegister::RegNum)i))
         {
         assignedRegisters += machine->getRealRegister((TR::RealRegister::RegNum)i)->getHasBeenAssignedInMethod() ? 1 : 0;
         }
      }

   return assignedRegisters;
   }

TR::RealRegister::RegNum getSingleAssignedRegister(TR::Machine *machine, const TR::ARMLinkageProperties *linkage)
   {
   for (int i = TR::RealRegister::LastGPR; i; i--)
      if (linkage->getPreserved((TR::RealRegister::RegNum)i) &&
          machine->getRealRegister((TR::RealRegister::RegNum)i)->getHasBeenAssignedInMethod())
         return (TR::RealRegister::RegNum) i;
   return (TR::RealRegister::RegNum) -1;
   }

// OLD FRAME SHAPE                       NEW FRAME SHAPE
//
// +                          +          +                          +
// | caller's frame           |          | caller's frame           |
// +==========================+ <-+ BP   +==========================+
// | locals                   |   |      | return address*          |
// | register saves           |   |      +--------------------------+ <-+ BP
// | outgoing arguments       | size     | locals                   |   |
// | callee's return address* |   |      | register saves           | size
// | (hole)*                  |   |      | outgoing arguments       |   |
// +==========================+ <-+ SP   +==========================+ <-+ SP
// | callee's frame           |          | caller's frame           |
//
// * Linkage slots are not needed in leaf methods. (TODO)

void TR::ARMPrivateLinkage::createPrologue(TR::Instruction *cursor)
   {
   TR::CodeGenerator   *codeGen    = cg();
   TR::Machine         *machine    = codeGen->machine();
   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   TR::RealRegister    *stackPtr   = machine->getRealRegister(properties.getStackPointerRegister());
   TR::RealRegister    *metaBase   = codeGen->getMethodMetaDataRegister();
   TR::Node               *firstNode  = comp()->getStartTree()->getNode();
   int i;

   const TR::ARMLinkageProperties& linkage = getProperties();

   int32_t localSize               = -((int32_t)bodySymbol->getLocalMappingCursor());
   int32_t intRegistersSaved       = getAssignedRegisterCount(machine, &linkage);
   int32_t registerSaveDescription = getAssignedRegisterList(machine, &linkage);

   TR::RealRegister::RegNum lastSavedFPR = TR::RealRegister::LastFPR;
   while (lastSavedFPR >= TR::RealRegister::fp8 &&
          !machine->getRealRegister(lastSavedFPR)->getHasBeenAssignedInMethod())
      {
      lastSavedFPR = (TR::RealRegister::RegNum)((uint32_t)lastSavedFPR - 1);
      }

   // TODO Fit GPR and FPR save data into (lower) 16 bits of descriptor.
   int32_t fpRegistersSaved = (int32_t)lastSavedFPR + 1 - (int32_t)TR::RealRegister::fp8;

   int32_t registerSaveSize = intRegistersSaved * 4 + fpRegistersSaved * 8;

   int32_t outgoingArgSize  = properties.getOffsetToFirstParm() + codeGen->getLargestOutgoingArgSize();
   int32_t totalFrameSize   = localSize + registerSaveSize + outgoingArgSize;

   // Align frame to 8-byte boundaries.
   if (debug("alignStackFrame"))
      totalFrameSize += ((totalFrameSize & 4) ? 4 : 0);

   int32_t resudialSize = totalFrameSize + properties.getOffsetToFirstLocal();

   codeGen->setFrameSizeInBytes(resudialSize);

   // Put offset to saved registers in top 16 bit of descriptor.
   int32_t offsetToSavedRegisters = localSize + properties.getOffsetToFirstLocal() + registerSaveSize;
   TR_ASSERT(offsetToSavedRegisters < 65536,
          "offset to saved registers from base of stack frame must be less than 2^16\n");
   registerSaveDescription |= (offsetToSavedRegisters << 16);
   codeGen->setRegisterSaveDescription(registerSaveDescription);

   if (comp()->getOption(TR_EntryBreakPoints))
      {
      cursor = new (trHeapMemory()) TR::Instruction(cursor, ARMOp_bad, firstNode, codeGen);
      }

   // TODO Only save arguments if full-speed debugging is enabled; otherwise, they
   // TODO should be moved from the registers in which they reside to the ones in
   // TODO which the method body expects them. See copyParametersToHomeLocation in
   // TODO the AMD64 codegen.
   //
   cursor = saveArguments(cursor);

   TR::RealRegister    *gr4    = machine->getRealRegister(TR::RealRegister::gr4);
   TR::RealRegister    *gr5    = machine->getRealRegister(TR::RealRegister::gr5);
   TR::RealRegister    *gr11   = machine->getRealRegister(TR::RealRegister::gr11);
   TR::RealRegister    *gr14   = machine->getRealRegister(TR::RealRegister::gr14);
   TR::MemoryReference *tempMR = NULL;
   uint32_t base, rotate;

   // Save return address, decrement the stack pointer, and check for stack
   // overflow.
   //
   if (!codeGen->getSnippetList().empty() ||
       bodySymbol->isEHAware() ||
       machine->getLinkRegisterKilled())
      {
      tempMR = new (trHeapMemory()) TR::MemoryReference(stackPtr, -4, codeGen);
      cursor = generateMemSrc1Instruction(codeGen, ARMOp_str, firstNode, tempMR, gr14, cursor);
      }

   if (constantIsImmed8r(totalFrameSize, &base, &rotate))
      {
      cursor = generateTrg1Src1ImmInstruction(codeGen, ARMOp_sub, firstNode, stackPtr, stackPtr, base, rotate, cursor);
      }
   else
      {
      cursor = armLoadConstant(firstNode, totalFrameSize, gr11, codeGen, cursor);
      cursor = generateTrg1Src2Instruction(codeGen, ARMOp_sub, firstNode, stackPtr, stackPtr, gr11, cursor);
      }

   if (!comp()->isDLT())
      {
      tempMR = new (trHeapMemory()) TR::MemoryReference(metaBase, codeGen->getStackLimitOffset(), codeGen);
      cursor = generateTrg1MemInstruction(codeGen, ARMOp_ldr, firstNode, gr4, tempMR, cursor);
      cursor = generateSrc2Instruction(codeGen, ARMOp_cmp, firstNode, stackPtr, gr4, cursor);

      TR::LabelSymbol *snippetLabel = generateLabelSymbol(codeGen);
      TR::LabelSymbol *restartLabel = generateLabelSymbol(codeGen);

      cursor = generateConditionalBranchInstruction(codeGen, firstNode, ARMConditionCodeLS, snippetLabel, cursor);

      TR::Snippet *snippet = new (trHeapMemory()) TR::ARMStackCheckFailureSnippet(codeGen, cursor->getNode(), restartLabel, snippetLabel);
      snippet->resetNeedsExceptionTableEntry();
      codeGen->addSnippet(snippet);

      cursor = generateLabelInstruction(codeGen, ARMOp_label, firstNode, restartLabel, cursor);
      }

   // Save preserved registers.
   // outgoingArgSize <= 1020 because the JVM spec limits the max number of method param to 255, so constantIsImmed8r can handle outgoingArgSize.
   if (intRegistersSaved)
      {
      if (1 == intRegistersSaved)
         {
         TR::Register *reg = machine->getRealRegister(getSingleAssignedRegister(machine, &linkage));
         tempMR = new (trHeapMemory()) TR::MemoryReference(stackPtr, outgoingArgSize, codeGen);
         cursor = generateMemSrc1Instruction(codeGen, ARMOp_str, firstNode, tempMR, reg, cursor);
         }
      else
         {
         constantIsImmed8r(outgoingArgSize, &base, &rotate);
         cursor = generateTrg1Src1ImmInstruction(codeGen, ARMOp_add, firstNode, gr4, stackPtr, base, rotate, cursor);
         // FIXME Need generateMultiMoveInstruction().
         cursor = new (trHeapMemory()) TR::ARMMultipleMoveInstruction(cursor, ARMOp_stm, firstNode, gr4, getAssignedRegisterList(machine, &linkage), codeGen);
         }
      }

   // for saving preserved VFP registers, the offset can be larger than 1024, so constantIsImmed10 and constantIsImmed8r might not be able to handle the offset.
   outgoingArgSize += intRegistersSaved * 4;
   if (fpRegistersSaved > 0)
      {
      if (1 == fpRegistersSaved)
         {
         TR::Register *reg = machine->getRealRegister(lastSavedFPR);
         if (constantIsImmed10(outgoingArgSize))
            {
            tempMR = new (trHeapMemory()) TR::MemoryReference(stackPtr, outgoingArgSize, codeGen);
            }
         else
            {
            cursor = armLoadConstant(firstNode, outgoingArgSize, gr4, codeGen, cursor);
            cursor = generateTrg1Src2Instruction(codeGen, ARMOp_add, firstNode, gr4, stackPtr, gr4, cursor);
            tempMR = new (trHeapMemory()) TR::MemoryReference(gr4, 0, codeGen);
            }
         cursor = generateMemSrc1Instruction(codeGen, ARMOp_fstd, firstNode, tempMR, reg, cursor);
         }
      else
         {
         if (constantIsImmed8r(outgoingArgSize, &base, &rotate))
            {
            cursor = generateTrg1Src1ImmInstruction(codeGen, ARMOp_add, firstNode, gr4, stackPtr, base, rotate, cursor);
            }
         else
            {
            cursor = armLoadConstant(firstNode, outgoingArgSize, gr4, codeGen, cursor);
            cursor = generateTrg1Src2Instruction(codeGen, ARMOp_add, firstNode, gr4, stackPtr, gr4, cursor);
            }
         // FIXME Need generateMultiMoveInstruction().
         uint16_t offset = ((TR::RealRegister::fp8 - TR::RealRegister::FirstFPR) << 12) | (fpRegistersSaved << 1);
         cursor = new (trHeapMemory()) TR::ARMMultipleMoveInstruction(cursor, ARMOp_fstmd, firstNode, gr4, offset, codeGen);
         ((TR::ARMMultipleMoveInstruction *)cursor)->setIncrement();
         }
      }

   // Initialize reference-type locals to NULL.
   TR::GCStackAtlas *atlas = codeGen->getStackAtlas();
   if (atlas)
      {
      uint32_t numLocalsToBeInitialized = atlas->getNumberOfSlotsToBeInitialized();

      // TODO Support internal pointers.
      if (numLocalsToBeInitialized > 0)
         {
         uint32_t offset = resudialSize + atlas->getLocalBaseOffset();
         cursor = armLoadConstant(firstNode, NULLVALUE, gr11, codeGen, cursor);

         if (numLocalsToBeInitialized > 0)
            {
#if 0
            traceMsg("%d locals to be initialized in %s\n",
                   numLocalsToBeInitialized,
                   signature(TR::comp()->getCurrentMethod()));
#endif
            // Inline zero initialization if the number of stores is small;
            // otherwise, set up a loop to perform the initialization.
            if (numLocalsToBeInitialized > 8)
               {
               TR::LabelSymbol *loopLabel = generateLabelSymbol(codeGen);
               cursor = armLoadConstant(firstNode, offset, gr4, codeGen, cursor);
               cursor = generateTrg1Src2Instruction(codeGen, ARMOp_add, firstNode, gr4, gr4, stackPtr, cursor);
               cursor = armLoadConstant(firstNode, (numLocalsToBeInitialized - 1) << 2, gr5, codeGen, cursor);
               cursor = generateLabelInstruction(codeGen, ARMOp_label, firstNode, loopLabel, cursor);
               tempMR = new (trHeapMemory()) TR::MemoryReference(gr4, gr5, codeGen);
               cursor = generateMemSrc1Instruction(codeGen, ARMOp_str, firstNode, tempMR, gr11, cursor);
               cursor = generateTrg1Src1ImmInstruction(codeGen, ARMOp_sub_r, firstNode, gr5, gr5, 4, 0, cursor);
               cursor = generateConditionalBranchInstruction(codeGen, firstNode, ARMConditionCodeGE, loopLabel, cursor);
               }
            else
               {
               for (uint32_t i = 0; i < numLocalsToBeInitialized; i++, offset += 4)
                  {
                  tempMR = new (trHeapMemory()) TR::MemoryReference(stackPtr, offset, codeGen);
                  cursor = generateMemSrc1Instruction(codeGen, ARMOp_str, firstNode, tempMR, gr11, cursor);
                  }
               }
            }
         }

      if (atlas->getInternalPointerMap())
         {
         int32_t offset = resudialSize + atlas->getOffsetOfFirstInternalPointer();

         // First collect all pinning arrays that are the base for at least
         // one derived internal pointer stack slot
         //
         int32_t numDistinctPinningArrays = 0;
         List<TR_InternalPointerPair> seenInternalPtrPairs(trMemory());
         List<TR::AutomaticSymbol> seenPinningArrays(trMemory());
         ListIterator<TR_InternalPointerPair> internalPtrIt(&atlas->getInternalPointerMap()->getInternalPointerPairs());
         for (TR_InternalPointerPair *internalPtrPair = internalPtrIt.getFirst(); internalPtrPair; internalPtrPair = internalPtrIt.getNext())
            {
            bool seenPinningArrayBefore = false;
            ListIterator<TR_InternalPointerPair> seenInternalPtrIt(&seenInternalPtrPairs);
            for (TR_InternalPointerPair *seenInternalPtrPair = seenInternalPtrIt.getFirst(); seenInternalPtrPair && (seenInternalPtrPair != internalPtrPair); seenInternalPtrPair = seenInternalPtrIt.getNext())
               {
               if (internalPtrPair->getPinningArrayPointer() == seenInternalPtrPair->getPinningArrayPointer())
                  {
                  seenPinningArrayBefore = true;
                  break;
                  }
               }

            if (!seenPinningArrayBefore)
               {
               seenPinningArrays.add(internalPtrPair->getPinningArrayPointer());
               seenInternalPtrPairs.add(internalPtrPair);
               numDistinctPinningArrays++;
               }
            }

         // Now collect all pinning arrays that are the base for only
         // internal pointers in registers
         //
         ListIterator<TR::AutomaticSymbol> autoIt(&atlas->getPinningArrayPtrsForInternalPtrRegs());
         TR::AutomaticSymbol *autoSymbol;
         for (autoSymbol = autoIt.getFirst(); autoSymbol != NULL; autoSymbol = autoIt.getNext())
            {
            if (!seenPinningArrays.find(autoSymbol))
               {
               seenPinningArrays.add(autoSymbol);
               numDistinctPinningArrays++;
               }
            }

         // Total number of slots to be initialized is number of pinning arrays +
         // number of derived internal pointer stack slots
         //
         int32_t numSlotsToBeInitialized = numDistinctPinningArrays + atlas->getInternalPointerMap()->getNumInternalPointers();

         if ((numSlotsToBeInitialized > 0) && !(numLocalsToBeInitialized > 0))
            {
            cursor = armLoadConstant(firstNode, NULLVALUE, gr11, codeGen, cursor);
            }

         for (i = 0; i < numSlotsToBeInitialized; i++, offset += TR::Compiler->om.sizeofReferenceAddress())
            {
            tempMR = new (trHeapMemory()) TR::MemoryReference(stackPtr, offset, codeGen);
            cursor = generateMemSrc1Instruction(codeGen, ARMOp_str, firstNode, tempMR, gr11, cursor);
            }
         }

      // FIXME Remove calls to debug("hasFramePointer"). The VM no longer
      // FIXME supports the use of a frame pointer. The TR_ASSERT() message
      // FIXME below seems to have been inherited from the PPC codegen, and
      // FIXME may be completely irrelevant.
      //
      if (!debug("hasFramePointer"))
         {
         // TR_ASSERT(totalFrameSize <= 1028, "Setting up a frame pointer anyway.");
         ListIterator<TR::AutomaticSymbol>  automaticIterator(&bodySymbol->getAutomaticList());
         TR::AutomaticSymbol *localCursor = automaticIterator.getFirst();
         while (localCursor)
            {
            localCursor->setOffset(localCursor->getOffset() + totalFrameSize);
            localCursor = automaticIterator.getNext();
            }

         ListIterator<TR::ParameterSymbol> parameterIterator(&bodySymbol->getParameterList());
         TR::ParameterSymbol *parmCursor = parameterIterator.getFirst();
         while (parmCursor)
            {
            parmCursor->setParameterOffset(parmCursor->getParameterOffset() + totalFrameSize);
            parmCursor = parameterIterator.getNext();
            }
         }

      TR_GCStackMap *map = atlas->getLocalMap();
      map->setLowestOffsetInstruction(cursor);
      if (!comp()->useRegisterMaps())
         atlas->addStackMap(map);
      }
   }

void TR::ARMPrivateLinkage::createEpilogue(TR::Instruction *cursor)
   {
   TR::CodeGenerator   *codeGen    = cg();
   TR::Machine         *machine    = codeGen->machine();
   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   TR::RealRegister    *stackPtr   = machine->getRealRegister(properties.getStackPointerRegister());
   TR::Node               *lastNode   = cursor->getNode();

   const TR::ARMLinkageProperties &linkage = getProperties();

   int32_t localSize         = -((int32_t)bodySymbol->getLocalMappingCursor());
   int32_t intRegistersSaved = getAssignedRegisterCount(machine, &linkage);

   TR::RealRegister::RegNum lastSavedFPR = TR::RealRegister::LastFPR;
   while (lastSavedFPR >= TR::RealRegister::fp8 &&
          !machine->getRealRegister(lastSavedFPR)->getHasBeenAssignedInMethod())
      {
      lastSavedFPR = (TR::RealRegister::RegNum)((uint32_t)lastSavedFPR - 1);
      }

   // TODO Fit GPR and FPR save data into (lower) 16 bits of descriptor.
   int32_t fpRegistersSaved = (int32_t)lastSavedFPR + 1 - (int32_t)TR::RealRegister::fp8;

   int32_t registerSaveSize  = intRegistersSaved * 4 + fpRegistersSaved * 8;

   int32_t outgoingArgSize   = properties.getOffsetToFirstParm() + codeGen->getLargestOutgoingArgSize();
   int32_t totalFrameSize    = localSize + registerSaveSize + outgoingArgSize;

   if (debug("alignStackFrame"))
      totalFrameSize += ((totalFrameSize & 4) ? 4 : 0);

   // Reload preserved registers.
   TR::MemoryReference *tempMR;
   uint32_t               base, rotate;

   if (intRegistersSaved)
      {
      if (1 == intRegistersSaved)
         {
         TR::RealRegister *reg = machine->getRealRegister(getSingleAssignedRegister(machine, &linkage));
         tempMR = new (trHeapMemory()) TR::MemoryReference(stackPtr, outgoingArgSize, codeGen);
         cursor = generateTrg1MemInstruction(codeGen, ARMOp_ldr, lastNode, reg, tempMR, cursor);
         }
      else
         {
         TR::RealRegister *gr4 = machine->getRealRegister(TR::RealRegister::gr4);
         constantIsImmed8r(outgoingArgSize, &base, &rotate);
         cursor = generateTrg1Src1ImmInstruction(codeGen, ARMOp_add, lastNode, gr4, stackPtr, base, rotate, cursor);
         cursor = new (trHeapMemory()) TR::ARMMultipleMoveInstruction(cursor, ARMOp_ldm, lastNode, gr4, getAssignedRegisterList(machine, &linkage), codeGen);
         }
      }

   outgoingArgSize += intRegistersSaved * 4;
   if (fpRegistersSaved > 0)
      {
      TR::RealRegister *gr4 = machine->getRealRegister(TR::RealRegister::gr4);
      if (1 == fpRegistersSaved)
         {
         TR::Register *reg = machine->getRealRegister(lastSavedFPR);
         if (constantIsImmed10(outgoingArgSize))
            {
            tempMR = new (trHeapMemory()) TR::MemoryReference(stackPtr, outgoingArgSize, codeGen);
            }
         else
            {
            cursor = armLoadConstant(lastNode, outgoingArgSize, gr4, codeGen, cursor);
            cursor = generateTrg1Src2Instruction(codeGen, ARMOp_add, lastNode, gr4, stackPtr, gr4, cursor);
            tempMR = new (trHeapMemory()) TR::MemoryReference(gr4, 0, codeGen);
            }
         cursor = generateMemSrc1Instruction(codeGen, ARMOp_fldd, lastNode, tempMR, reg, cursor);
         }
      else
         {
         if (constantIsImmed8r(outgoingArgSize, &base, &rotate))
            {
            cursor = generateTrg1Src1ImmInstruction(codeGen, ARMOp_add, lastNode, gr4, stackPtr, base, rotate, cursor);
            }
         else
            {
            cursor = armLoadConstant(lastNode, outgoingArgSize, gr4, codeGen, cursor);
            cursor = generateTrg1Src2Instruction(codeGen, ARMOp_add, lastNode, gr4, stackPtr, gr4, cursor);
            }
         // FIXME Need generateMultiMoveInstruction().
         uint16_t offset = ((TR::RealRegister::fp8 - TR::RealRegister::FirstFPR) << 12) | (fpRegistersSaved << 1);
         cursor = new (trHeapMemory()) TR::ARMMultipleMoveInstruction(cursor, ARMOp_fldmd, lastNode, gr4, offset, codeGen);
         ((TR::ARMMultipleMoveInstruction *)cursor)->setIncrement();
         }
      }

   // Reload return address.
   TR::RealRegister *gr14 = machine->getRealRegister(TR::RealRegister::gr14);
   TR::RealRegister *gr15 = machine->getRealRegister(TR::RealRegister::gr15);

   if (codeGen->getSnippetList().size() > 1 ||
       (comp()->isDLT() && !codeGen->getSnippetList().empty()) ||
       bodySymbol->isEHAware()                 ||
       machine->getLinkRegisterKilled())
      {
      tempMR = new (trHeapMemory()) TR::MemoryReference(stackPtr, totalFrameSize + properties.getOffsetToFirstLocal(), codeGen);
      cursor = generateTrg1MemInstruction(codeGen, ARMOp_ldr, lastNode, gr14, tempMR, cursor);
      }

   // Collapse stack frame and return.
   if (constantIsImmed8r(totalFrameSize, &base, &rotate))
      {
      cursor = generateTrg1Src1ImmInstruction(codeGen, ARMOp_add, lastNode, stackPtr, stackPtr, base, rotate, cursor);
      }
   else
      {
      TR::RealRegister *gr4 = machine->getRealRegister(TR::RealRegister::gr4);
      cursor = armLoadConstant(lastNode, totalFrameSize, gr4, codeGen, cursor);
      cursor = generateTrg1Src2Instruction(codeGen, ARMOp_add, lastNode, stackPtr, stackPtr, gr4, cursor);
      }

   cursor = generateTrg1Src1Instruction(codeGen, ARMOp_mov, lastNode, gr15, gr14, cursor);
   }

TR::MemoryReference *TR::ARMPrivateLinkage::getOutgoingArgumentMemRef(int32_t               totalSize,
                                                                       int32_t               offset,
                                                                       TR::Register          *argReg,
                                                                       TR_ARMOpCodes         opCode,
                                                                       TR::ARMMemoryArgument &memArg)
   {
#ifdef DEBUG_ARM_LINKAGE
printf("private: totalSize %d offset %d\n", totalSize, offset); fflush(stdout);
#endif

   int32_t                spOffset = totalSize - offset - TR::Compiler->om.sizeofReferenceAddress();
   TR::RealRegister    *sp       = cg()->machine()->getRealRegister(properties.getStackPointerRegister());
   TR::MemoryReference *result   = new (trHeapMemory()) TR::MemoryReference(sp, spOffset, cg());
   memArg.argRegister = argReg;
   memArg.argMemory   = result;
   memArg.opCode      = opCode;
   return result;
   }

int32_t TR::ARMPrivateLinkage::buildArgs(TR::Node                            *callNode,
                                        TR::RegisterDependencyConditions *dependencies,
                                        TR::Register* &vftReg,
                                        bool                                isVirtual)
   {
   return buildARMLinkageArgs(callNode, dependencies, vftReg, TR_Private, isVirtual);
   }

void TR::ARMPrivateLinkage::buildVirtualDispatch(TR::Node *callNode,
                        TR::RegisterDependencyConditions *dependencies,
                        TR::RegisterDependencyConditions *postDeps,
                        TR::Register                     *vftReg,
                        uint32_t                         sizeOfArguments)
   {
   TR::CodeGenerator *codeGen = cg();
   TR::Machine       *machine = codeGen->machine();

#ifdef LOCK_R14
   /* Dependency #0 is for vftReg. */
   TR::Register        *gr11 = dependencies->searchPreConditionRegister(TR::RealRegister::gr11);
   TR::Register        *gr0  = dependencies->searchPreConditionRegister(TR::RealRegister::gr0);
   TR::Register        *gr4  = dependencies->searchPreConditionRegister(TR::RealRegister::gr4);
   TR::RealRegister *gr14 = machine->getRealRegister(TR::RealRegister::gr14);
   TR::RealRegister *gr15 = machine->getRealRegister(TR::RealRegister::gr15);
#else
   TR::Register        *gr11 = dependencies->searchPreConditionRegister(TR::RealRegister::gr11);
   TR::Register        *gr14 = dependencies->searchPreConditionRegister(TR::RealRegister::gr14);
   TR::Register        *gr0  = dependencies->searchPreConditionRegister(TR::RealRegister::gr0);
   TR::Register        *gr4  = dependencies->searchPreConditionRegister(TR::RealRegister::gr4);
   TR::RealRegister *gr15 = machine->getRealRegister(TR::RealRegister::gr15);
#endif

   TR::SymbolReference  *methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   TR::LabelSymbol      *doneLabel    = NULL;
   TR::Instruction      *gcPoint;

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());

   // Computed calls
   //
   if (methodSymbol->isComputed())
      {
      void *thunk;

      switch (methodSymbol->getMandatoryRecognizedMethod())
         {
         case TR::java_lang_invoke_ComputedCalls_dispatchVirtual:
            {
            // Need a j2i thunk for the method that will ultimately be dispatched by this handle call
            char    *j2iSignature = fej9->getJ2IThunkSignatureForDispatchVirtual(methodSymbol->getMethod()->signatureChars(), methodSymbol->getMethod()->signatureLength(), comp());
            int32_t  signatureLen = strlen(j2iSignature);
            thunk = fej9->getJ2IThunk(j2iSignature, signatureLen, comp());
            if (!thunk)
               {
               thunk = fej9->setJ2IThunk(j2iSignature, signatureLen,
                                         TR::ARMCallSnippet::generateVIThunk(fej9->getEquivalentVirtualCallNodeForDispatchVirtual(callNode, comp()), sizeOfArguments, codeGen), comp());
               }
            }
         default:
            if (fej9->needsInvokeExactJ2IThunk(callNode, comp()))
               {
               comp()->getPersistentInfo()->getInvokeExactJ2IThunkTable()->addThunk(
                  TR::ARMCallSnippet::generateInvokeExactJ2IThunk(callNode, sizeOfArguments, codeGen, methodSymbol->getMethod()->signatureChars()), fej9);
               }
            break;
         }

      TR::Node *child = callNode->getFirstChild();
      TR::Register *targetAddress = codeGen->evaluate(child);
      if (targetAddress->getRegisterPair())
         {
         // On 32-bit, we can just ignore the top 32 bits of the 64-bit target address
         targetAddress = targetAddress->getLowOrder();
         }
      doneLabel = generateLabelSymbol(codeGen);
      generateTrg1Src1Instruction(codeGen, ARMOp_mov, callNode, gr4, targetAddress);

      TR::Instruction *instr = generateTrg1Src1Instruction(codeGen, ARMOp_mov, callNode, gr14, gr15);
      instr->setDependencyConditions(dependencies);
      dependencies->incRegisterTotalUseCounts(codeGen);

      gcPoint = generateTrg1Src1Instruction(codeGen, ARMOp_mov, callNode, gr15, gr4);

      gcPoint->ARMNeedsGCMap(getProperties().getPreservedRegisterMapForGC());

      generateLabelInstruction(codeGen, ARMOp_label, callNode, doneLabel, postDeps);

      return;
      }

   uint8_t *thunk = (uint8_t*)fej9->getJ2IThunk(methodSymbol->getMethod()->signatureChars(), methodSymbol->getMethod()->signatureLength(), comp());
   if (!thunk)
      thunk = (uint8_t*)fej9->setJ2IThunk(methodSymbol->getMethod()->signatureChars(), methodSymbol->getMethod()->signatureLength(), TR::ARMCallSnippet::generateVIThunk(callNode, sizeOfArguments, codeGen), comp());

   if (methodSymbol->isVirtual())
      {
      TR::Register * classReg;
      if (methodSymRef == comp()->getSymRefTab()->findObjectNewInstanceImplSymbol())
         {//In this case, methodSymRef is resolved and VTable index is small enough to fit in 12bit, so we can safely use gr11 for classReg.
         if (TR::Compiler->cls.classesOnHeap())
            {
            classReg = gr11;
            generateTrg1MemInstruction(codeGen, ARMOp_ldr, callNode, gr11, new (trHeapMemory()) TR::MemoryReference(gr0, fej9->getOffsetOfClassFromJavaLangClassField(), codeGen));
            }
         else
            classReg = gr0;
         }
      else
         {
         classReg = vftReg;
         }
      TR::MemoryReference *tempMR;
      if (methodSymRef->isUnresolved() || comp()->compileRelocatableCode())
         {
         doneLabel = generateLabelSymbol(codeGen);

         TR::LabelSymbol *snippetLabel = generateLabelSymbol(codeGen);
         codeGen->addSnippet(new (trHeapMemory()) TR::ARMVirtualUnresolvedSnippet(codeGen,
                                                                callNode,
                                                                snippetLabel,
                                                                sizeOfArguments,
                                                                doneLabel,
                                                                thunk));

         // These 5 instructions will be ultimately modified to load the
         // method pointer and move return address to link register.
         // The 4 instruction before branch should be a no-op if the offset turns
         // out to be within range (for most cases).
         generateTrg1ImmInstruction(codeGen, ARMOp_mov, callNode, gr11, 0, 0);
         generateTrg1Src1ImmInstruction(codeGen, ARMOp_add, callNode, gr11, gr11, 0, 8);
         generateTrg1Src1ImmInstruction(codeGen, ARMOp_add, callNode, gr11, gr11, 0, 16);
         generateTrg1Src1ImmInstruction(codeGen, ARMOp_add, callNode, gr11, gr11, 0, 24);

         generateLabelInstruction(codeGen, ARMOp_b, callNode, snippetLabel, dependencies);

         tempMR = new (trHeapMemory()) TR::MemoryReference(classReg, 0, codeGen);
         gcPoint = generateTrg1MemInstruction(codeGen, ARMOp_ldr, callNode, gr15, tempMR);
         }
      else
         {
         int32_t offset = methodSymRef->getOffset();
         if (constantIsImmed12(offset))
            {
            TR::Instruction *instr = generateTrg1Src1Instruction(codeGen, ARMOp_mov, callNode, gr14, gr15);
            instr->setDependencyConditions(dependencies);
            dependencies->incRegisterTotalUseCounts(codeGen);

            tempMR = new (trHeapMemory()) TR::MemoryReference(classReg, offset, codeGen);
            gcPoint = generateTrg1MemInstruction(codeGen, ARMOp_ldr, callNode, gr15, tempMR);
            gcPoint->setDependencyConditions(postDeps);
            postDeps->incRegisterTotalUseCounts(codeGen);
            }
         else
            {
            TR::Instruction *instr = armLoadConstant(callNode, offset, gr11, codeGen);
            instr = generateTrg1Src1Instruction(codeGen, ARMOp_mov, callNode, gr14, gr15);
            instr->setDependencyConditions(dependencies);
            dependencies->incRegisterTotalUseCounts(codeGen);

            tempMR = new (trHeapMemory()) TR::MemoryReference(classReg, gr11, 0, codeGen);
            gcPoint = generateTrg1MemInstruction(codeGen, ARMOp_ldr, callNode, gr15, tempMR);
            gcPoint->setDependencyConditions(postDeps);
            postDeps->incRegisterTotalUseCounts(codeGen);
            }
         }
      }
   else
      {
      // TODO inline interface dispatch and IPIC
      doneLabel = generateLabelSymbol(codeGen);

      TR::LabelSymbol *snippetLabel = generateLabelSymbol(codeGen);
      codeGen->addSnippet(new (trHeapMemory()) TR::ARMInterfaceCallSnippet(codeGen,
                                                         callNode,
                                                         snippetLabel,
                                                         sizeOfArguments,
                                                         doneLabel,
                                                         thunk));

      gcPoint = generateLabelInstruction(codeGen, ARMOp_b, callNode, snippetLabel, dependencies);
      }

   gcPoint->ARMNeedsGCMap(getProperties().getPreservedRegisterMapForGC());

   // Insert a return label if a snippet is used. The register dependency
   // pre-conditions must be anchored on the branch out, and the post-
   // conditions must be anchored on the return label.
   if (doneLabel)
      generateLabelInstruction(codeGen, ARMOp_label, callNode, doneLabel, postDeps);

   return;
   }

TR::Register *TR::ARMPrivateLinkage::buildDirectDispatch(TR::Node *callNode)
   {
   TR::MethodSymbol *callSym = callNode->getSymbol()->castToMethodSymbol();
   if (callSym->isJNI() &&
       callNode->isPreparedForDirectJNI())
      {
      callSym->setLinkage(TR_J9JNILinkage);
      TR::Linkage *linkage = cg()->getLinkage(callSym->getLinkageConvention());

      return linkage->buildDirectDispatch(callNode);
      }
   else
      {
      return buildARMLinkageDirectDispatch(callNode);
      }
   }

TR::Register *TR::ARMPrivateLinkage::buildIndirectDispatch(TR::Node *callNode)
   {
   TR::CodeGenerator *codeGen = cg();
   TR::Machine       *machine = codeGen->machine();

   const TR::ARMLinkageProperties &pp = getProperties();
   TR::RegisterDependencyConditions *deps =
      new (trHeapMemory()) TR::RegisterDependencyConditions(pp.getNumberOfDependencyGPRegisters() + 8 /*pp.getNumFloatArgRegs()*/,
                                             pp.getNumberOfDependencyGPRegisters() + 8 /* pp.getNumFloatArgRegs()*/, trMemory());

   TR::Register *vftReg = NULL;
   int32_t argSize = buildArgs(callNode, deps, vftReg, true);
   deps->stopAddingConditions();

   TR::RegisterDependencyConditions *postDeps = deps->clone(cg());

   deps->setNumPostConditions(0, trMemory());
   postDeps->setNumPreConditions(0, trMemory());

#ifdef LOCK_R14
   /* Dependency #0 is for vftReg. */
   TR::Register        *gr11 = deps->searchPreConditionRegister(TR::RealRegister::gr11);
   TR::Register        *gr0  = deps->searchPreConditionRegister(TR::RealRegister::gr0);
   TR::RealRegister *gr14 = machine->getRealRegister(TR::RealRegister::gr14);
   TR::RealRegister *gr15 = machine->getRealRegister(TR::RealRegister::gr15);
#else
   TR::Register        *gr11 = deps->searchPreConditionRegister(TR::RealRegister::gr11);
   TR::Register        *gr14 = deps->searchPreConditionRegister(TR::RealRegister::gr14);
   TR::Register        *gr0  = deps->searchPreConditionRegister(TR::RealRegister::gr0);
   TR::RealRegister *gr15 = machine->getRealRegister(TR::RealRegister::gr15);
#endif

   TR::Register        *returnRegister;
   TR::DataType resType = callNode->getType();

   buildVirtualDispatch(callNode, deps, postDeps, vftReg, argSize);

   codeGen->machine()->setLinkRegisterKilled(true);
   codeGen->setHasCall();

   switch(callNode->getOpCodeValue())
      {
      case TR::icalli:
      case TR::acalli:
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
      case TR::fcalli:
#endif
         returnRegister = postDeps->searchPostConditionRegister(pp.getIntegerReturnRegister());
         if (resType.isFloatingPoint())
            {
            TR::Register *tempReg = codeGen->allocateSinglePrecisionRegister();
            TR::Instruction *cursor = generateTrg1Src1Instruction(codeGen, ARMOp_fmsr, callNode, tempReg, returnRegister);
            returnRegister = tempReg;
            }
         break;
      case TR::lcalli:
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
      case TR::dcalli:
#endif
         {
         TR::Register *lowReg  = postDeps->searchPostConditionRegister(pp.getLongLowReturnRegister());
         TR::Register *highReg = postDeps->searchPostConditionRegister(pp.getLongHighReturnRegister());
         returnRegister = codeGen->allocateRegisterPair(lowReg, highReg);
         if (resType.isDouble())
            {
            TR::Register *tempReg = codeGen->allocateRegister(TR_FPR);
            TR::Instruction *cursor = generateTrg1Src2Instruction(codeGen, ARMOp_fmdrr, callNode, tempReg, lowReg, highReg);
            returnRegister = tempReg;
            }

         }
         break;
#if !defined(__VFP_FP__) || defined(__SOFTFP__)
      case TR::fcalli:
      case TR::dcalli:
         returnRegister = postDeps->searchPostConditionRegister(pp.getFloatReturnRegister());
         break;
#endif
      case TR::calli:
         returnRegister = NULL;
         break;
      default:
         returnRegister = NULL;
         TR_ASSERT(0, "Unknown indirect call Opcode.");
      }

   callNode->setRegister(returnRegister);
   return returnRegister;
   }

int32_t TR::ARMHelperLinkage::buildArgs(TR::Node                            *callNode,
                                       TR::RegisterDependencyConditions *dependencies,
					TR::Register* &vftReg,
                                       bool                                isVirtual)
   {
   TR_ASSERT(!isVirtual, "virtual helper calls not supported");
   return buildARMLinkageArgs(callNode, dependencies, vftReg, TR_Helper, isVirtual);
   }
