/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <algorithm>
#include <iterator>

#include "codegen/ARM64Instruction.hpp"
#include "codegen/ARM64OutOfLineCodeSection.hpp"
#include "codegen/ARM64PrivateLinkage.hpp"
#include "codegen/CallSnippet.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/StackCheckFailureSnippet.hpp"
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/J2IThunk.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/StackMemoryRegion.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "runtime/Runtime.hpp"

#define MIN_PROFILED_CALL_FREQUENCY (.075f)
#define MAX_PROFILED_CALL_FREQUENCY (.90f)

uint32_t J9::ARM64::PrivateLinkage::_globalRegisterNumberToRealRegisterMap[] =
   {
   // GPRs
   TR::RealRegister::x15,
   TR::RealRegister::x14,
   TR::RealRegister::x13,
   TR::RealRegister::x12,
   TR::RealRegister::x11,
   TR::RealRegister::x10,
   TR::RealRegister::x9,
   TR::RealRegister::x8, // indirect result location register
   TR::RealRegister::x18, // platform register
   // callee-saved registers
   TR::RealRegister::x28,
   TR::RealRegister::x27,
   TR::RealRegister::x26,
   TR::RealRegister::x25,
   TR::RealRegister::x24,
   TR::RealRegister::x23,
   TR::RealRegister::x22,
   TR::RealRegister::x21,
   // parameter registers
   TR::RealRegister::x7,
   TR::RealRegister::x6,
   TR::RealRegister::x5,
   TR::RealRegister::x4,
   TR::RealRegister::x3,
   TR::RealRegister::x2,
   TR::RealRegister::x1,
   TR::RealRegister::x0,

   // FPRs
   TR::RealRegister::v31,
   TR::RealRegister::v30,
   TR::RealRegister::v29,
   TR::RealRegister::v28,
   TR::RealRegister::v27,
   TR::RealRegister::v26,
   TR::RealRegister::v25,
   TR::RealRegister::v24,
   TR::RealRegister::v23,
   TR::RealRegister::v22,
   TR::RealRegister::v21,
   TR::RealRegister::v20,
   TR::RealRegister::v19,
   TR::RealRegister::v18,
   TR::RealRegister::v17,
   TR::RealRegister::v16,
   // callee-saved registers
   TR::RealRegister::v15,
   TR::RealRegister::v14,
   TR::RealRegister::v13,
   TR::RealRegister::v12,
   TR::RealRegister::v11,
   TR::RealRegister::v10,
   TR::RealRegister::v9,
   TR::RealRegister::v8,
   // parameter registers
   TR::RealRegister::v7,
   TR::RealRegister::v6,
   TR::RealRegister::v5,
   TR::RealRegister::v4,
   TR::RealRegister::v3,
   TR::RealRegister::v2,
   TR::RealRegister::v1,
   TR::RealRegister::v0
   };

J9::ARM64::PrivateLinkage::PrivateLinkage(TR::CodeGenerator *cg)
   : J9::PrivateLinkage(cg),
   _interpretedMethodEntryPoint(NULL),
   _jittedMethodEntryPoint(NULL)
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

   _properties._registerFlags[TR::RealRegister::x18]   = 0;

   _properties._registerFlags[TR::RealRegister::x19]   = Preserved|ARM64_Reserved; // vmThread
   _properties._registerFlags[TR::RealRegister::x20]   = Preserved|ARM64_Reserved; // Java SP

   for (i = TR::RealRegister::x21; i <= TR::RealRegister::x28; i++)
      _properties._registerFlags[i] = Preserved; // x21 - x28 Preserved

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

   std::copy(std::begin(_globalRegisterNumberToRealRegisterMap), std::end(_globalRegisterNumberToRealRegisterMap), std::begin(_properties._allocationOrder));

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
   _properties._computedCallTargetRegister  = TR::RealRegister::x8;
   _properties._vtableIndexArgumentRegister = TR::RealRegister::x9;
   _properties._j9methodArgumentRegister    = TR::RealRegister::x0;

   // Volatile GPR (0-15, 18) + FPR (0-31) + VFT Reg
   _properties._numberOfDependencyGPRegisters = 17 + 32 + 1;
   setOffsetToFirstParm(0);
   _properties._offsetToFirstLocal            = -8;
   }

TR::ARM64LinkageProperties& J9::ARM64::PrivateLinkage::getProperties()
   {
   return _properties;
   }

uint32_t J9::ARM64::PrivateLinkage::getRightToLeft()
   {
   return getProperties().getRightToLeft();
   }

intptr_t
J9::ARM64::PrivateLinkage::entryPointFromCompiledMethod()
   {
   return reinterpret_cast<intptr_t>(getJittedMethodEntryPoint()->getBinaryEncoding());
   }

intptr_t
J9::ARM64::PrivateLinkage::entryPointFromInterpretedMethod()
   {
   return reinterpret_cast<intptr_t>(getInterpretedMethodEntryPoint()->getBinaryEncoding());
   }

void J9::ARM64::PrivateLinkage::alignLocalReferences(uint32_t &stackIndex)
   {
   TR::Compilation *comp = self()->comp();
   TR::GCStackAtlas *atlas = self()->cg()->getStackAtlas();
   const int32_t localObjectAlignment = TR::Compiler->om.getObjectAlignmentInBytes();
   const uint8_t pointerSize = TR::Compiler->om.sizeofReferenceAddress();

   if (comp->useCompressedPointers())
      {
      if (comp->getOption(TR_TraceCG))
         {
         traceMsg(comp,"\nLOCAL OBJECT ALIGNMENT: stack offset before alignment: %d,", stackIndex);
         }

      // stackIndex in mapCompactedStack is calculated using only local reference sizes and does not include the padding
      stackIndex -= pointerSize * atlas->getNumberOfPaddingSlots();

      if (comp->getOption(TR_TraceCG))
         {
         traceMsg(comp," with padding: %d,", stackIndex);
         }
      // If there are any local objects we have to make sure they are aligned properly
      // when compressed pointers are used.  Otherwise, pointer compression may clobber
      // part of the pointer.
      //
      // Each auto's GC index will have already been aligned, so just the starting stack
      // offset needs to be aligned.
      //
      uint32_t unalignedStackIndex = stackIndex;
      stackIndex &= ~(localObjectAlignment - 1);
      uint32_t paddingBytes = unalignedStackIndex - stackIndex;
      if (paddingBytes > 0)
         {
         TR_ASSERT_FATAL((paddingBytes & (pointerSize - 1)) == 0, "Padding bytes should be a multiple of the slot/pointer size");
         uint32_t paddingSlots = paddingBytes / pointerSize;
         atlas->setNumberOfSlotsMapped(atlas->getNumberOfSlotsMapped() + paddingSlots);
         }
      }
   }

void J9::ARM64::PrivateLinkage::mapStack(TR::ResolvedMethodSymbol *method)
   {
   if (self()->cg()->getLocalsIG() && self()->cg()->getSupportsCompactedLocals())
      {
      mapCompactedStack(method);
      return;
      }

   const TR::ARM64LinkageProperties& linkageProperties = getProperties();
   int32_t firstLocalOffset = linkageProperties.getOffsetToFirstLocal();
   uint32_t stackIndex = firstLocalOffset;
   int32_t lowGCOffset = stackIndex;

   TR::GCStackAtlas *atlas = cg()->getStackAtlas();

   // Map all garbage collected references together so can concisely represent
   // stack maps. They must be mapped so that the GC map index in each local
   // symbol is honoured.
   //
   uint32_t numberOfLocalSlotsMapped = atlas->getNumberOfSlotsMapped() - atlas->getNumberOfParmSlotsMapped();

   stackIndex -= numberOfLocalSlotsMapped * TR::Compiler->om.sizeofReferenceAddress();

   if (comp()->useCompressedPointers())
      {
      // If there are any local objects we have to make sure they are aligned properly
      // when compressed pointers are used.  Otherwise, pointer compression may clobber
      // part of the pointer.
      //
      // Each auto's GC index will have already been aligned, so just the starting stack
      // offset needs to be aligned.
      //
      uint32_t unalignedStackIndex = stackIndex;
      stackIndex &= ~(TR::Compiler->om.getObjectAlignmentInBytes() - 1);
      uint32_t paddingBytes = unalignedStackIndex - stackIndex;
      if (paddingBytes > 0)
         {
         TR_ASSERT((paddingBytes & (TR::Compiler->om.sizeofReferenceAddress() - 1)) == 0, "Padding bytes should be a multiple of the slot/pointer size");
         uint32_t paddingSlots = paddingBytes / TR::Compiler->om.sizeofReferenceAddress();
         atlas->setNumberOfSlotsMapped(atlas->getNumberOfSlotsMapped() + paddingSlots);
         }
      }

   ListIterator<TR::AutomaticSymbol> automaticIterator(&method->getAutomaticList());
   TR::AutomaticSymbol *localCursor;
   int32_t firstLocalGCIndex = atlas->getNumberOfParmSlotsMapped();

   // Map local references to set the stack position correct according to the GC map index
   //
   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      {
      if (localCursor->getGCMapIndex() >= 0)
         {
         localCursor->setOffset(stackIndex + TR::Compiler->om.sizeofReferenceAddress() * (localCursor->getGCMapIndex() - firstLocalGCIndex));
         if (localCursor->getGCMapIndex() == atlas->getIndexOfFirstInternalPointer())
            {
            atlas->setOffsetOfFirstInternalPointer(localCursor->getOffset() - firstLocalOffset);
            }
         }
      }

   method->setObjectTempSlots((lowGCOffset - stackIndex) / TR::Compiler->om.sizeofReferenceAddress());
   lowGCOffset = stackIndex;

   // Now map the rest of the locals
   //
   automaticIterator.reset();
   localCursor = automaticIterator.getFirst();

   while (localCursor != NULL)
      {
      if (localCursor->getGCMapIndex() < 0 &&
          localCursor->getSize() != 8)
         {
         mapSingleAutomatic(localCursor, stackIndex);
         }

      localCursor = automaticIterator.getNext();
      }

   automaticIterator.reset();
   localCursor = automaticIterator.getFirst();

   while (localCursor != NULL)
      {
      if (localCursor->getGCMapIndex() < 0 &&
          localCursor->getSize() == 8)
         {
         stackIndex -= (stackIndex & 0x4)?4:0;
         mapSingleAutomatic(localCursor, stackIndex);
         }

      localCursor = automaticIterator.getNext();
      }

   method->setLocalMappingCursor(stackIndex);

   mapIncomingParms(method);

   atlas->setLocalBaseOffset(lowGCOffset - firstLocalOffset);
   atlas->setParmBaseOffset(atlas->getParmBaseOffset() + getOffsetToFirstParm() - firstLocalOffset);
   }

void J9::ARM64::PrivateLinkage::mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex)
   {
   mapSingleAutomatic(p, p->getRoundedSize(), stackIndex);
   }

void J9::ARM64::PrivateLinkage::mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t size, uint32_t &stackIndex)
   {
   /*
    * Align stack-allocated objects that don't have GC map index > 0.
    */
   if (comp()->useCompressedPointers() && p->isLocalObject() && (p->getGCMapIndex() == -1))
      {
      int32_t roundup = TR::Compiler->om.getObjectAlignmentInBytes() - 1;

      size = (size  + roundup) & (~roundup);
      }

   p->setOffset(stackIndex -= size);
   }

static void lockRegister(TR::RealRegister *regToAssign)
   {
   regToAssign->setState(TR::RealRegister::Locked);
   regToAssign->setAssignedRegister(regToAssign);
   }

void J9::ARM64::PrivateLinkage::initARM64RealRegisterLinkage()
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

   // assign "maximum" weight to registers x21-x28
   for (icount = TR::RealRegister::x21; icount <= TR::RealRegister::x28; icount++)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000);

   // assign "maximum" weight to registers v0-v31
   for (icount = TR::RealRegister::v0; icount <= TR::RealRegister::v31; icount++)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000);
   }


void
J9::ARM64::PrivateLinkage::setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method)
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


int32_t
J9::ARM64::PrivateLinkage::calculatePreservedRegisterSaveSize(
      uint32_t &registerSaveDescription,
      uint32_t &numGPRsSaved)
   {
   TR::Machine *machine = cg()->machine();

   TR::RealRegister::RegNum firstPreservedGPR = TR::RealRegister::x21;
   TR::RealRegister::RegNum lastPreservedGPR = TR::RealRegister::x28;

   // Create a bit vector of preserved registers that have been modified
   // in this method.
   //
   for (int32_t i = firstPreservedGPR; i <= lastPreservedGPR; i++)
      {
      if (machine->getRealRegister((TR::RealRegister::RegNum)i)->getHasBeenAssignedInMethod())
         {
         registerSaveDescription |= 1 << (i-1);
         numGPRsSaved++;
         }
      }

   return numGPRsSaved*8;
   }

/**
 * @brief Generates instructions for initializing local variable and internal pointer slots in prologue
 *
 * @param[in] cursor                          : instruction cursor
 * @param[in] numSlotsToBeInitialized         : number of slots to be initialized
 * @param[in] offsetToFirstSlotFromAdjustedSP : offset to first slot from adjusted Java SP
 * @param[in] zeroReg                         : zero register (x31)
 * @param[in] baseReg                         : base register (x10)
 * @param[in] javaSP                          : Java SP register (x20)
 * @param[in] cg                              : Code Generator
 *
 * @return instruction cursor
 */
static TR::Instruction* initializeLocals(TR::Instruction *cursor, uint32_t numSlotsToBeInitialized, int32_t offsetToFirstSlotFromAdjustedSP,
                              TR::RealRegister *zeroReg, TR::RealRegister *baseReg, TR::RealRegister *javaSP, TR::CodeGenerator *cg)
   {
   auto loopCount = numSlotsToBeInitialized / 2;
   // stp instruction has 7bit immediate offset which is scaled by 8 for 64bit registers.
   // If the offset to the last 2 slots cleared by stp instruction does not fit in imm7,
   // we use x10 as base register.
   const bool isImm7OffsetOverflow = (loopCount > 0) &&
         !constantIsImm7((offsetToFirstSlotFromAdjustedSP + (loopCount - 1) * 2 * TR::Compiler->om.sizeofReferenceAddress()) >> 3);

   auto offset = offsetToFirstSlotFromAdjustedSP;
   if (isImm7OffsetOverflow)
      {
      if (!constantIsImm7(offset >> 3))
         {
         // If offset does not fit in imm7, update baseReg and reset offset to 0
         if (constantIsUnsignedImm12(offset))
            {
            cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addimmx, NULL, baseReg, javaSP, offset, cursor);
            }
         else
            {
            cursor = loadConstant32(cg, NULL, offset, baseReg, cursor);
            cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::addx, NULL, baseReg, javaSP, baseReg, cursor);
            }
         offset = 0;
         }
      else
         {
         // mov baseReg, javaSP
         cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::orrx, NULL, baseReg, zeroReg, javaSP, cursor);
         }


      for (int32_t i = 0; i < loopCount; i++)
         {
         if (!constantIsImm7(offset >> 3))
            {
            // If offset does not fit in imm7, update baseReg and reset offset to 0
            cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addimmx, NULL, baseReg, baseReg, offset, cursor);
            offset = 0;
            }
         TR::MemoryReference *localMR = TR::MemoryReference::createWithDisplacement(cg, baseReg, offset);
         cursor = generateMemSrc2Instruction(cg, TR::InstOpCode::stpoffx, NULL, localMR, zeroReg, zeroReg, cursor);
         offset += (TR::Compiler->om.sizeofReferenceAddress() * 2);
         }
      if (numSlotsToBeInitialized % 2)
         {
         // clear residue
         TR::MemoryReference *localMR = TR::MemoryReference::createWithDisplacement(cg, baseReg, offset);
         cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::strimmx, NULL, localMR, zeroReg, cursor);
         }
      }
   else
      {
      for (int32_t i = 0; i < loopCount; i++, offset += (TR::Compiler->om.sizeofReferenceAddress() * 2))
         {
         TR::MemoryReference *localMR = TR::MemoryReference::createWithDisplacement(cg, javaSP, offset);
         cursor = generateMemSrc2Instruction(cg, TR::InstOpCode::stpoffx, NULL, localMR, zeroReg, zeroReg, cursor);
         }
      if (numSlotsToBeInitialized % 2)
         {
         // clear residue
         TR::MemoryReference *localMR = TR::MemoryReference::createWithDisplacement(cg, javaSP, offset);
         cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::strimmx, NULL, localMR, zeroReg, cursor);
         }
      }

   return cursor;
   }

void J9::ARM64::PrivateLinkage::createPrologue(TR::Instruction *cursor)
   {

   // Prologues are emitted post-RA so it is fine to use real registers directly
   // in instructions
   //
   TR::ARM64LinkageProperties& properties = getProperties();
   TR::Machine *machine = cg()->machine();
   TR::RealRegister *vmThread = machine->getRealRegister(properties.getMethodMetaDataRegister());   // x19
   TR::RealRegister *javaSP = machine->getRealRegister(properties.getStackPointerRegister());       // x20

   TR::Instruction *beforeInterpreterMethodEntryPointInstruction = cursor;

   // --------------------------------------------------------------------------
   // Create the entry point when transitioning from an interpreted method.
   // Parameters are passed on the stack, so load them into the appropriate
   // linkage registers expected by the JITed method entry point.
   //
   cursor = loadStackParametersToLinkageRegisters(cursor);

   TR::Instruction *beforeJittedMethodEntryPointInstruction = cursor;

   // Entry breakpoint
   //
   if (comp()->getOption(TR_EntryBreakPoints))
      {
      cursor = generateExceptionInstruction(cg(), TR::InstOpCode::brkarm64, NULL, 0, cursor);
      }

   // --------------------------------------------------------------------------
   // Determine the bitvector of registers to preserve in the prologue
   //
   uint32_t registerSaveDescription = 0;
   uint32_t numGPRsSaved = 0;

   uint32_t preservedRegisterSaveSize = calculatePreservedRegisterSaveSize(registerSaveDescription, numGPRsSaved);

   // Offset between the entry JavaSP of a method and the first mapped local.  This covers
   // the space needed to preserve the RA.  It is a negative (or zero) offset.
   //
   int32_t firstLocalOffset = properties.getOffsetToFirstLocal();

   // The localMappingCursor is a negative-offset mapping of locals (autos and spills) to
   // the stack relative to the entry JavaSP of a method.  It includes the offset to the
   // first mapped local.
   //
   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   int32_t localsSize = -(int32_t)(bodySymbol->getLocalMappingCursor());

   // Size of the frame needed to handle the argument storage requirements of any method
   // call in the current method.
   //
   // The offset to the first parm is the offset between the entry JavaSP and the first
   // mapped parameter.  It is a positive (or zero) offset.
   //
   int32_t outgoingArgsSize = cg()->getLargestOutgoingArgSize() + getOffsetToFirstParm();

   int32_t frameSizeIncludingReturnAddress = preservedRegisterSaveSize + localsSize + outgoingArgsSize;

   // Align the frame to 16 bytes
   //
   int32_t alignedFrameSizeIncludingReturnAddress = (frameSizeIncludingReturnAddress + 15) & ~15;

   // The frame size maintained by the code generator does not include the RA
   //
   cg()->setFrameSizeInBytes(alignedFrameSizeIncludingReturnAddress + firstLocalOffset);

   // --------------------------------------------------------------------------
   // Encode register save description (RSD)
   //
   int32_t preservedRegisterOffsetFromJavaBP = (alignedFrameSizeIncludingReturnAddress - outgoingArgsSize + firstLocalOffset);

   TR_ASSERT_FATAL(preservedRegisterOffsetFromJavaBP >= 0, "expecting a positive preserved register area offset");

   // Frame size is too large for the RSD word in the metadata
   //
   if (preservedRegisterOffsetFromJavaBP > 0xffff)
      {
      comp()->failCompilation<TR::CompilationInterrupted>("Overflowed or underflowed bounds of regSaveOffset in calculateFrameSize.");
      }

   registerSaveDescription |= (preservedRegisterOffsetFromJavaBP & 0xffff);

   cg()->setRegisterSaveDescription(registerSaveDescription);

   // In FSD, we must save linkage regs to the incoming argument area because
   // the stack overflow check doesn't preserve them.
   bool parmsHaveBeenStored = false;
   if (comp()->getOption(TR_FullSpeedDebug))
      {
      cursor = saveParametersToStack(cursor);
      parmsHaveBeenStored = true;
      }

   // --------------------------------------------------------------------------
   // Store return address (RA)
   //
   TR::MemoryReference *returnAddressMR = TR::MemoryReference::createWithDisplacement(cg(), javaSP, firstLocalOffset);
   cursor = generateMemSrc1Instruction(cg(), TR::InstOpCode::sturx, NULL, returnAddressMR, machine->getRealRegister(TR::RealRegister::lr), cursor);

   // --------------------------------------------------------------------------
   // Speculatively adjust Java SP with the needed frame size.
   // This includes the preserved RA slot.
   //
   if (constantIsUnsignedImm12(alignedFrameSizeIncludingReturnAddress))
      {
      cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::subimmx, NULL, javaSP, javaSP, alignedFrameSizeIncludingReturnAddress, cursor);
      }
   else
      {
      TR::RealRegister *x9Reg = machine->getRealRegister(TR::RealRegister::RegNum::x9);

      if (constantIsUnsignedImm16(alignedFrameSizeIncludingReturnAddress))
         {
         // x9 will contain the aligned frame size
         //
         cursor = loadConstant32(cg(), NULL, alignedFrameSizeIncludingReturnAddress, x9Reg, cursor);
         cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::subx, NULL, javaSP, javaSP, x9Reg, cursor);
         }
      else
         {
         TR_ASSERT_FATAL(0, "Large frame size not supported in prologue yet");
         }
      }

   // --------------------------------------------------------------------------
   // Perform javaSP overflow check
   //
   if (!comp()->isDLT())
      {
      //    if (javaSP < vmThread->SOM)
      //       goto stackOverflowSnippetLabel
      //
      // stackOverflowRestartLabel:
      //
      TR::MemoryReference *somMR = TR::MemoryReference::createWithDisplacement(cg(), vmThread, cg()->getStackLimitOffset());
      TR::RealRegister *somReg = machine->getRealRegister(TR::RealRegister::RegNum::x10);
      cursor = generateTrg1MemInstruction(cg(), TR::InstOpCode::ldrimmx, NULL, somReg, somMR, cursor);

      TR::RealRegister *zeroReg = machine->getRealRegister(TR::RealRegister::xzr);
      cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::subsx, NULL, zeroReg, javaSP, somReg, cursor);

      TR::LabelSymbol *stackOverflowSnippetLabel = generateLabelSymbol(cg());
      cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::b_cond, NULL, stackOverflowSnippetLabel, TR::CC_LS, cursor);

      TR::LabelSymbol *stackOverflowRestartLabel = generateLabelSymbol(cg());
      cursor = generateLabelInstruction(cg(), TR::InstOpCode::label, NULL, stackOverflowRestartLabel, cursor);

      cg()->addSnippet(new (cg()->trHeapMemory()) TR::ARM64StackCheckFailureSnippet(cg(), NULL, stackOverflowRestartLabel, stackOverflowSnippetLabel));
      }
   else
      {
      // If StackCheckFailureSnippet is not added to the end of the snippet list and no data snippets exist,
      // we might have a HelperCallSnippet at the end of the method.
      // HelperCallSnippets add a GCMap to the instruction next to the `bl` instruction to the helper,
      // and if a HelperCallSnippet is at the end of the method, GCMap is added to the address beyond the range of the method.
      // To avoid that, we add a dummy ConstantDataSnippet. (Data snippets are emitted after normal snippets.)
      if (!cg()->hasDataSnippets())
         {
         auto snippet = cg()->findOrCreate4ByteConstant(NULL, 0);
         snippet->setReloType(TR_NoRelocation);
         }
      }

   // --------------------------------------------------------------------------
   // Preserve GPRs
   //
   // javaSP has been adjusted, so preservedRegs start at offset outgoingArgSize
   // relative to the javaSP
   //
   // Registers are preserved in order from x21 (low memory) -> x28 (high memory)
   //
   if (numGPRsSaved)
      {
      TR::RealRegister::RegNum firstPreservedGPR = TR::RealRegister::x21;
      TR::RealRegister::RegNum lastPreservedGPR = TR::RealRegister::x28;

      int32_t preservedRegisterOffsetFromJavaSP = outgoingArgsSize;

      for (TR::RealRegister::RegNum regIndex = firstPreservedGPR; regIndex <= lastPreservedGPR; regIndex=(TR::RealRegister::RegNum)((uint32_t)regIndex+1))
         {
         TR::RealRegister *preservedRealReg = machine->getRealRegister(regIndex);
         if (preservedRealReg->getHasBeenAssignedInMethod())
            {
            TR::MemoryReference *preservedRegMR = TR::MemoryReference::createWithDisplacement(cg(), javaSP, preservedRegisterOffsetFromJavaSP);
            cursor = generateMemSrc1Instruction(cg(), TR::InstOpCode::strimmx, NULL, preservedRegMR, preservedRealReg, cursor);
            preservedRegisterOffsetFromJavaSP += 8;
            numGPRsSaved--;
            }
         }

      TR_ASSERT_FATAL(numGPRsSaved == 0, "preserved register mismatch in prologue");
      }

   // --------------------------------------------------------------------------
   // Initialize locals
   //
   TR::GCStackAtlas *atlas = cg()->getStackAtlas();
   if (atlas)
      {
      // The GC stack maps are conservative in that they all say that
      // collectable locals are live. This means that these locals must be
      // cleared out in case a GC happens before they are allocated a valid
      // value.
      // The atlas contains the number of locals that need to be cleared. They
      // are all mapped together starting at GC index 0.
      //
      uint32_t numLocalsToBeInitialized = atlas->getNumberOfSlotsToBeInitialized();
      if (numLocalsToBeInitialized > 0 || atlas->getInternalPointerMap())
         {
         // The LocalBaseOffset and firstLocalOffset are either negative or zero values
         //
         int32_t initializedLocalsOffsetFromAdjustedJavaSP = alignedFrameSizeIncludingReturnAddress + atlas->getLocalBaseOffset() + firstLocalOffset;

         TR::RealRegister *zeroReg = machine->getRealRegister(TR::RealRegister::RegNum::xzr);
         TR::RealRegister *baseReg = machine->getRealRegister(TR::RealRegister::RegNum::x10);

         cursor = initializeLocals(cursor, numLocalsToBeInitialized, initializedLocalsOffsetFromAdjustedJavaSP, 
                              zeroReg, baseReg, javaSP, cg());

         if (atlas->getInternalPointerMap())
            {
            // Total number of slots to be initialized is number of pinning arrays +
            // number of derived internal pointer stack slots
            //
            int32_t numSlotsToBeInitialized = atlas->getNumberOfDistinctPinningArrays() + atlas->getInternalPointerMap()->getNumInternalPointers();
            int32_t offsetToFirstInternalPointerFromAdjustedJavaSP = alignedFrameSizeIncludingReturnAddress + atlas->getOffsetOfFirstInternalPointer() + firstLocalOffset;

            cursor = initializeLocals(cursor, numSlotsToBeInitialized, offsetToFirstInternalPointerFromAdjustedJavaSP, 
                              zeroReg, baseReg, javaSP, cg());
            }
         }
      }

   // Adjust final offsets on locals and parm symbols now that the frame size is known.
   // These offsets are relative to the javaSP which has been adjusted downward to
   // accommodate the frame of this method.
   //
   ListIterator<TR::AutomaticSymbol> automaticIterator(&bodySymbol->getAutomaticList());
   TR::AutomaticSymbol *localCursor = automaticIterator.getFirst();

   while (localCursor != NULL)
      {
      localCursor->setOffset(localCursor->getOffset() + alignedFrameSizeIncludingReturnAddress);
      localCursor = automaticIterator.getNext();
      }

   ListIterator<TR::ParameterSymbol> parameterIterator(&bodySymbol->getParameterList());
   TR::ParameterSymbol *parmCursor = parameterIterator.getFirst();
   while (parmCursor != NULL)
      {
      parmCursor->setParameterOffset(parmCursor->getParameterOffset() + alignedFrameSizeIncludingReturnAddress);
      parmCursor = parameterIterator.getNext();
      }

   // Ensure arguments reside where the method body expects them to be (either in registers or
   // on the stack).  This state is influenced by global register assignment.
   //
   cursor = copyParametersToHomeLocation(cursor, parmsHaveBeenStored);

   // Set the instructions for method entry points
   setInterpretedMethodEntryPoint(beforeInterpreterMethodEntryPointInstruction->getNext());
   setJittedMethodEntryPoint(beforeJittedMethodEntryPointInstruction->getNext());
   }

void J9::ARM64::PrivateLinkage::createEpilogue(TR::Instruction *cursor)
   {
   const TR::ARM64LinkageProperties& properties = getProperties();
   TR::Machine *machine = cg()->machine();
   TR::Node *lastNode = cursor->getNode();
   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   TR::RealRegister *javaSP = machine->getRealRegister(properties.getStackPointerRegister()); // x20

   // restore preserved GPRs
   int32_t preservedRegisterOffsetFromJavaSP = cg()->getLargestOutgoingArgSize() + getOffsetToFirstParm(); // outgoingArgsSize
   TR::RealRegister::RegNum firstPreservedGPR = TR::RealRegister::x21;
   TR::RealRegister::RegNum lastPreservedGPR = TR::RealRegister::x28;
   for (TR::RealRegister::RegNum r = firstPreservedGPR; r <= lastPreservedGPR; r = (TR::RealRegister::RegNum)((uint32_t)r+1))
      {
      TR::RealRegister *rr = machine->getRealRegister(r);
      if (rr->getHasBeenAssignedInMethod())
         {
         TR::MemoryReference *preservedRegMR = TR::MemoryReference::createWithDisplacement(cg(), javaSP, preservedRegisterOffsetFromJavaSP);
         cursor = generateTrg1MemInstruction(cg(), TR::InstOpCode::ldrimmx, lastNode, rr, preservedRegMR, cursor);
         preservedRegisterOffsetFromJavaSP += 8;
         }
      }

   // remove space for preserved registers
   int32_t firstLocalOffset = properties.getOffsetToFirstLocal();

   uint32_t alignedFrameSizeIncludingReturnAddress = cg()->getFrameSizeInBytes() - firstLocalOffset;
   if (constantIsUnsignedImm12(alignedFrameSizeIncludingReturnAddress))
      {
      cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::addimmx, lastNode, javaSP, javaSP, alignedFrameSizeIncludingReturnAddress, cursor);
      }
   else
      {
      TR::RealRegister *x9Reg = machine->getRealRegister(TR::RealRegister::RegNum::x9);
      cursor = loadConstant32(cg(), lastNode, alignedFrameSizeIncludingReturnAddress, x9Reg, cursor);
      cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::addx, lastNode, javaSP, javaSP, x9Reg, cursor);
      }

   // restore return address
   TR::RealRegister *lr = machine->getRealRegister(TR::RealRegister::lr);
   if (machine->getLinkRegisterKilled())
      {
      TR::MemoryReference *returnAddressMR = TR::MemoryReference::createWithDisplacement(cg(), javaSP, firstLocalOffset);
      cursor = generateTrg1MemInstruction(cg(), TR::InstOpCode::ldurx, lastNode, lr, returnAddressMR, cursor);
      }

   // return
   generateRegBranchInstruction(cg(), TR::InstOpCode::ret, lastNode, lr, cursor);
   }

void J9::ARM64::PrivateLinkage::pushOutgoingMemArgument(TR::Register *argReg, int32_t offset, TR::InstOpCode::Mnemonic opCode, TR::ARM64MemoryArgument &memArg)
   {
   const TR::ARM64LinkageProperties& properties = self()->getProperties();
   TR::RealRegister *javaSP = cg()->machine()->getRealRegister(properties.getStackPointerRegister()); // x20

   TR::MemoryReference *result = TR::MemoryReference::createWithDisplacement(cg(), javaSP, offset);
   memArg.argRegister = argReg;
   memArg.argMemory = result;
   memArg.opCode = opCode;
   }

int32_t J9::ARM64::PrivateLinkage::buildArgs(TR::Node *callNode,
   TR::RegisterDependencyConditions *dependencies)
   {
   return buildPrivateLinkageArgs(callNode, dependencies, TR_Private);
   }

int32_t J9::ARM64::PrivateLinkage::buildPrivateLinkageArgs(TR::Node *callNode,
   TR::RegisterDependencyConditions *dependencies,
   TR_LinkageConventions linkage)
   {
   TR_ASSERT(linkage == TR_Private || linkage == TR_Helper || linkage == TR_CHelper, "Unexpected linkage convention");

   const TR::ARM64LinkageProperties& properties = getProperties();
   TR::ARM64MemoryArgument *pushToMemory = NULL;
   TR::Register *tempReg;
   int32_t argIndex = 0;
   int32_t numMemArgs = 0;
   int32_t memArgSize = 0;
   int32_t firstExplicitArg = 0;
   int32_t from, to, step;
   int32_t argSize = -getOffsetToFirstParm();
   int32_t totalSize = 0;
   int32_t multiplier;

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
      case TR::com_ibm_jit_JITHelpers_dispatchVirtual:
         specialArgReg = getProperties().getVTableIndexArgumentRegister();
         break;
      }
   if (specialArgReg != TR::RealRegister::NoReg)
      {
      if (comp()->getOption(TR_TraceCG))
         {
         traceMsg(comp(), "Special arg %s in %s\n",
            comp()->getDebug()->getName(callNode->getChild(from)),
            comp()->getDebug()->getName(cg()->machine()->getRealRegister(specialArgReg)));
         }
      // Skip the special arg in the first loop
      from += step;
      }

   // C helpers have an implicit first argument (the VM thread) that we have to account for
   if (linkage == TR_CHelper)
      {
      TR_ASSERT(numIntArgRegs > 0, "This code doesn't handle passing this implicit arg on the stack");
      numIntegerArgs++;
      totalSize += TR::Compiler->om.sizeofReferenceAddress();
      }

   for (int32_t i = from; (rightToLeft && i >= to) || (!rightToLeft && i <= to); i += step)
      {
      child = callNode->getChild(i);
      childType = child->getDataType();

      switch (childType)
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Int64:
         case TR::Address:
            multiplier = (childType == TR::Int64) ? 2 : 1;
            if (numIntegerArgs >= numIntArgRegs)
               {
               numMemArgs++;
               memArgSize += TR::Compiler->om.sizeofReferenceAddress() * multiplier;
               }
            numIntegerArgs++;
            totalSize += TR::Compiler->om.sizeofReferenceAddress() * multiplier;
            break;
         case TR::Float:
         case TR::Double:
            multiplier = (childType == TR::Double) ? 2 : 1;
            if (numFloatArgs >= numFloatArgRegs)
               {
               numMemArgs++;
               memArgSize += TR::Compiler->om.sizeofReferenceAddress() * multiplier;
               }
            numFloatArgs++;
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
      }

   if (specialArgReg)
      from -= step;  // we do want to process special args in the following loop

   numIntegerArgs = 0;
   numFloatArgs = 0;

   // C helpers have an implicit first argument (the VM thread) that we have to account for
   if (linkage == TR_CHelper)
      {
      TR_ASSERT(numIntArgRegs > 0, "This code doesn't handle passing this implicit arg on the stack");
      TR::Register *vmThreadArgRegister = cg()->allocateRegister();
      generateMovInstruction(cg(), callNode, vmThreadArgRegister, cg()->getMethodMetaDataRegister());
      dependencies->addPreCondition(vmThreadArgRegister, properties.getIntegerArgumentRegister(numIntegerArgs));
      if (resType.getDataType() == TR::NoType)
         dependencies->addPostCondition(vmThreadArgRegister, properties.getIntegerArgumentRegister(numIntegerArgs));
      numIntegerArgs++;
      firstExplicitArg = 1;
      }

   // Helper linkage preserves all argument registers except the return register
   // TODO: C helper linkage does not, this code needs to make sure argument registers are killed in post dependencies
   for (int32_t i = from; (rightToLeft && i >= to) || (!rightToLeft && i <= to); i += step)
      {
      TR::Register *argRegister;
      TR::InstOpCode::Mnemonic op;
      bool isSpecialArg = (i == from && specialArgReg != TR::RealRegister::NoReg);

      child = callNode->getChild(i);
      childType = child->getDataType();

      switch (childType)
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Int64:
         case TR::Address:
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
            if (isSpecialArg)
               {
               if (specialArgReg == properties.getIntegerReturnRegister(0))
                  {
                  TR::Register *resultReg;
                  if (resType.isAddress())
                     resultReg = cg()->allocateCollectedReferenceRegister();
                  else
                     resultReg = cg()->allocateRegister();
                  dependencies->addPreCondition(argRegister, specialArgReg);
                  dependencies->addPostCondition(resultReg, properties.getIntegerReturnRegister(0));
                  }
               else
                  {
                  TR::addDependency(dependencies, argRegister, specialArgReg, TR_GPR, cg());
                  }
               }
            else
               {
               argSize += TR::Compiler->om.sizeofReferenceAddress() * ((childType == TR::Int64) ? 2 : 1);
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  if (argRegister->containsCollectedReference())
                     tempReg = cg()->allocateCollectedReferenceRegister();
                  else
                     tempReg = cg()->allocateRegister();
                  generateMovInstruction(cg(), callNode, tempReg, argRegister);
                  argRegister = tempReg;
                  }
               if (numIntegerArgs < numIntArgRegs)
                  {
                  if (numIntegerArgs == firstExplicitArg)
                     {
                     // the first integer argument
                     TR::Register *resultReg;
                     if (resType.isAddress())
                        resultReg = cg()->allocateCollectedReferenceRegister();
                     else
                        resultReg = cg()->allocateRegister();
                     dependencies->addPreCondition(argRegister, properties.getIntegerArgumentRegister(numIntegerArgs));
                     dependencies->addPostCondition(resultReg, TR::RealRegister::x0);
                     if (firstExplicitArg == 1)
                        dependencies->addPostCondition(argRegister, properties.getIntegerArgumentRegister(numIntegerArgs));
                     }
                  else
                     {
                     TR::addDependency(dependencies, argRegister, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
                     }
                  }
               else // numIntegerArgs >= numIntArgRegs
                  {
                  op = ((childType == TR::Address) || (childType == TR::Int64)) ? TR::InstOpCode::strimmx : TR::InstOpCode::strimmw;
                  pushOutgoingMemArgument(argRegister, totalSize - argSize, op, pushToMemory[argIndex++]);
                  }
               numIntegerArgs++;
               }
            break;
         case TR::Float:
         case TR::Double:
            if (childType == TR::Float)
               {
               argSize += TR::Compiler->om.sizeofReferenceAddress();
               argRegister = pushFloatArg(child);
               }
            else
               {
               argSize += TR::Compiler->om.sizeofReferenceAddress() * 2;
               argRegister = pushDoubleArg(child);
               }
            if (!cg()->canClobberNodesRegister(child, 0))
               {
               tempReg = cg()->allocateRegister(TR_FPR);
               op = (childType == TR::Float) ? TR::InstOpCode::fmovs : TR::InstOpCode::fmovd;
               generateTrg1Src1Instruction(cg(), op, callNode, tempReg, argRegister);
               argRegister = tempReg;
               }
            if (numFloatArgs < numFloatArgRegs)
               {
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
               op = (childType == TR::Float) ? TR::InstOpCode::vstrimms : TR::InstOpCode::vstrimmd;
               pushOutgoingMemArgument(argRegister, totalSize - argSize, op, pushToMemory[argIndex++]);
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
      if (callSymbol->isComputed() && i == getProperties().getComputedCallTargetRegister())
         continue;
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

   // Helper linkage preserves all registers that are not argument registers, so we don't need to spill them.
   if (linkage != TR_Helper)
      {
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
      }

   /* Spills all vector registers */
   if ((linkage != TR_Helper) && killsVectorRegisters())
      {
      TR::Register *tmpReg = cg()->allocateRegister();
      dependencies->addPostCondition(tmpReg, TR::RealRegister::KillVectorRegs);
      cg()->stopUsingRegister(tmpReg);
      }

   if (numMemArgs > 0)
      {
      for (argIndex = 0; argIndex < numMemArgs; argIndex++)
         {
         TR::Register *aReg = pushToMemory[argIndex].argRegister;
         generateMemSrc1Instruction(cg(), pushToMemory[argIndex].opCode, callNode, pushToMemory[argIndex].argMemory, aReg);
         cg()->stopUsingRegister(aReg);
         }
      }

   return totalSize;
   }

void J9::ARM64::PrivateLinkage::buildDirectCall(TR::Node *callNode,
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

   bool forceUnresolvedDispatch = !fej9->isResolvedDirectDispatchGuaranteed(comp());

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

      if (callSymRef->isUnresolved() || comp()->compileRelocatableCode())
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

      // Nop is necessary due to confusion when resolving shared slots at a transition
      if (callSymRef->isOSRInductionHelper())
         cg()->generateNop(callNode);

      }

   gcPoint->ARM64NeedsGCMap(cg(), callSymbol->getLinkageConvention() == TR_Helper ? 0xffffffff : pp.getPreservedRegisterMapForGC());
   }

TR::Register *J9::ARM64::PrivateLinkage::buildDirectDispatch(TR::Node *callNode)
   {
   TR::SymbolReference *callSymRef = callNode->getSymbolReference();
   const TR::ARM64LinkageProperties &pp = getProperties();
   // Extra post dependency for killing vector registers (see KillVectorRegs)
   const int extraPostReg = killsVectorRegisters() ? 1 : 0;
   TR::RegisterDependencyConditions *dependencies =
      new (trHeapMemory()) TR::RegisterDependencyConditions(
         pp.getNumberOfDependencyGPRegisters(),
         pp.getNumberOfDependencyGPRegisters() + extraPostReg, trMemory());

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

   dependencies->stopUsingDepRegs(cg(), retReg);
   return retReg;
   }

/**
 * @brief Gets profiled call site information
 *
 * @param[in] cg:           code generator
 * @param[in] callNode:     node for call
 * @param[in] maxStaticPIC: maximum number of static PICs
 * @param[out] values:      list of PIC items
 * @returns true if any call site information is returned
 */
static bool getProfiledCallSiteInfo(TR::CodeGenerator *cg, TR::Node *callNode, uint32_t maxStaticPICs, TR_ScratchList<J9::ARM64PICItem> &values)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   if (comp->compileRelocatableCode())
      return false;

   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

   if (!methodSymbol->isVirtual() && !methodSymbol->isInterface())
      return false;

   TR_AddressInfo *info = static_cast<TR_AddressInfo*>(TR_ValueProfileInfoManager::getProfiledValueInfo(callNode, comp, AddressInfo));
   if (!info)
      {
      if (comp->getOption(TR_TraceCG))
         {
         traceMsg(comp, "Profiled target not found for node %p\n", callNode);
         }
      return false;
      }
   static const bool tracePIC = feGetEnv("TR_TracePIC") != NULL;
   if (tracePIC)
      {
      traceMsg(comp, "Value profile info for callNode %p in %s\n", callNode, comp->signature());
      info->getProfiler()->dumpInfo(comp->getOutFile());
      traceMsg(comp, "\n");
      }
   uint32_t totalFreq = info->getTotalFrequency();
   if (totalFreq == 0 || info->getTopProbability() < MIN_PROFILED_CALL_FREQUENCY)
      {
      if (comp->getOption(TR_TraceCG))
         {
         traceMsg(comp, "Profiled target with enough frequency not found for node %p\n", callNode);
         }
      return false;
      }

   TR_ScratchList<TR_ExtraAddressInfo> allValues(comp->trMemory());
   info->getSortedList(comp, &allValues);

   TR_ResolvedMethod   *owningMethod = methodSymRef->getOwningMethod(comp);
   TR_OpaqueClassBlock *callSiteMethodClass;

   if (methodSymbol->isVirtual())
       callSiteMethodClass = methodSymRef->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod()->classOfMethod();

   ListIterator<TR_ExtraAddressInfo> valuesIt(&allValues);

   uint32_t             numStaticPics = 0;
   TR_ExtraAddressInfo *profiledInfo;
   for (profiledInfo = valuesIt.getFirst();  numStaticPics < maxStaticPICs && profiledInfo != NULL; profiledInfo = valuesIt.getNext())
      {
      float freq = (float)profiledInfo->_frequency / totalFreq;
      if (freq < MIN_PROFILED_CALL_FREQUENCY)
         break;

      TR_OpaqueClassBlock *clazz = (TR_OpaqueClassBlock *)profiledInfo->_value;
      if (comp->getPersistentInfo()->isObsoleteClass(clazz, fej9))
         continue;

      TR_ResolvedMethod *method;

      if (methodSymbol->isVirtual())
         {
         TR_ASSERT_FATAL(callSiteMethodClass, "Expecting valid callSiteMethodClass for virtual call");
         if (!cg->isProfiledClassAndCallSiteCompatible(clazz, callSiteMethodClass))
            continue;

         method = owningMethod->getResolvedVirtualMethod(comp, clazz, methodSymRef->getOffset());
         }
      else
         {
         method = owningMethod->getResolvedInterfaceMethod(comp, clazz, methodSymRef->getCPIndex());
         }

      if (!method || method->isInterpreted())
         continue;

      values.add(new (comp->trStackMemory()) J9::ARM64PICItem(clazz, method, freq));
      ++numStaticPics;
      }

   return numStaticPics > 0;
   }

/**
 * @brief Generates instruction sequence for static PIC call
 *
 * @param[in] cg:             code generator
 * @param[in] callNode:       node for call
 * @param[in] profiledClass:  class suggested by interpreter profiler
 * @param[in] profiledMethod: method suggested by interpreter profiler
 * @param[in] vftReg:         register containing VFT
 * @param[in] tempReg:        temporary register
 * @param[in] missLabel:      label for cache miss
 * @param[in] regMapForGC:    register map for GC
 * @returns instruction making direct call to the method
 */
static TR::Instruction* buildStaticPICCall(TR::CodeGenerator *cg, TR::Node *callNode, TR_OpaqueClassBlock *profiledClass, TR_ResolvedMethod *profiledMethod,
                                             TR::Register *vftReg, TR::Register *tempReg, TR::LabelSymbol *missLabel, uint32_t regMapForGC)
   {
   TR::Compilation *comp = cg->comp();
   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::SymbolReference *profiledMethodSymRef = comp->getSymRefTab()->findOrCreateMethodSymbol(methodSymRef->getOwningMethodIndex(),
                                                                                             -1,
                                                                                             profiledMethod,
                                                                                             TR::MethodSymbol::Virtual);

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   if (comp->compileRelocatableCode())
      {
      loadAddressConstantInSnippet(cg, callNode, reinterpret_cast<intptr_t>(profiledClass), tempReg, TR_ClassPointer);
      }
   else
      {
      bool isUnloadAssumptionRequired = fej9->isUnloadAssumptionRequired(profiledClass, comp->getCurrentMethod());

      if (isUnloadAssumptionRequired)
         {
         loadAddressConstantInSnippet(cg, callNode, reinterpret_cast<intptr_t>(profiledClass), tempReg, TR_NoRelocation, true); 
         }
      else
         {
         loadAddressConstant(cg, callNode, reinterpret_cast<intptr_t>(profiledClass), tempReg, NULL, true);
         }
      }
   generateCompareInstruction(cg, callNode, vftReg, tempReg, true);

   generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, callNode, missLabel, TR::CC_NE);

   TR::Instruction *gcPoint = generateImmSymInstruction(cg, TR::InstOpCode::bl, callNode, (uintptr_t)profiledMethod->startAddressForJittedMethod(),
                                                                                  NULL, profiledMethodSymRef, NULL);
   gcPoint->ARM64NeedsGCMap(cg, regMapForGC);
   fej9->reserveTrampolineIfNecessary(comp, profiledMethodSymRef, false);
   return gcPoint;
   }

/**
 * @brief Generates instruction sequence for virtual call
 *
 * @param[in] cg:          code generator
 * @param[in] callNode:    node for the virtual call
 * @param[in] vftReg:      register containing VFT
 * @param[in] x9:          x9 register
 * @param[in] regMapForGC: register map for GC
 */
static void buildVirtualCall(TR::CodeGenerator *cg, TR::Node *callNode, TR::Register *vftReg, TR::Register *x9, uint32_t regMapForGC)
   {
   int32_t offset = callNode->getSymbolReference()->getOffset();
   TR_ASSERT_FATAL(offset < 0, "Unexpected positive offset for virtual call");

   // jitVTableIndex() in oti/JITInterface.hpp assumes the instruction sequence below
   if (offset >= -65536)
      {
      generateTrg1ImmInstruction(cg, TR::InstOpCode::movnx, callNode, x9, ~offset & 0xFFFF);
      }
   else
      {
      generateTrg1ImmInstruction(cg, TR::InstOpCode::movzx, callNode, x9, offset & 0xFFFF);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::movkx, callNode, x9,
                                 (((offset >> 16) & 0xFFFF) | TR::MOV_LSL16));
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sbfmx, callNode, x9, x9, 0x1F); // sxtw x9, w9
      }
   TR::MemoryReference *tempMR = TR::MemoryReference::createWithIndexReg(cg, vftReg, x9);
   generateTrg1MemInstruction(cg, TR::InstOpCode::ldroffx, callNode, x9, tempMR);
   TR::Instruction *gcPoint = generateRegBranchInstruction(cg, TR::InstOpCode::blr, callNode, x9);
   gcPoint->ARM64NeedsGCMap(cg, regMapForGC);
   }

/**
 * @brief Generates instruction sequence for interface call
 *
 * @param[in] cg:          code generator
 * @param[in] callNode:    node for the interface call
 * @param[in] vftReg:      vft register
 * @param[in] tmpReg:      temporary register
 * @param[in] ifcSnippet:  interface call snippet
 * @param[in] regMapForGC: register map for GC
 */
static void buildInterfaceCall(TR::CodeGenerator *cg, TR::Node *callNode, TR::Register *vftReg, TR::Register *tmpReg, TR::ARM64InterfaceCallSnippet *ifcSnippet, uint32_t regMapForGC)
   {
   /*
    *  Generating following instruction sequence.
    *  Recompilation is dependent on this instruction sequence.
    *  Please do not modify without changing recompilation code.
    *
    *  ldrx tmpReg, L_firstClassCacheSlot
    *  cmpx vftReg, tmpReg
    *  ldrx tmpReg, L_firstBranchAddressCacheSlot
    *  beq  hitLabel
    *  ldrx tmpReg, L_secondClassCacheSlot
    *  cmpx vftReg, tmpReg
    *  bne  snippetLabel
    *  ldrx tmpReg, L_secondBranchAddressCacheSlot
    * hitLabel:
    *  blr  tmpReg
    * doneLabel:
    */

   TR::LabelSymbol *ifcSnippetLabel = ifcSnippet->getSnippetLabel();
   TR::LabelSymbol *firstClassCacheSlotLabel = ifcSnippet->getFirstClassCacheSlotLabel();
   generateTrg1ImmSymInstruction(cg, TR::InstOpCode::ldrx, callNode, tmpReg, 0, firstClassCacheSlotLabel);

   TR::LabelSymbol *hitLabel = generateLabelSymbol(cg);
   generateCompareInstruction(cg, callNode, vftReg, tmpReg, true);
   TR::LabelSymbol *firstBranchAddressCacheSlotLabel = ifcSnippet->getFirstBranchAddressCacheSlotLabel();

   generateTrg1ImmSymInstruction(cg, TR::InstOpCode::ldrx, callNode, tmpReg, 0, firstBranchAddressCacheSlotLabel);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, callNode, hitLabel, TR::CC_EQ);

   TR::LabelSymbol *secondClassCacheSlotLabel = ifcSnippet->getSecondClassCacheSlotLabel();

   generateTrg1ImmSymInstruction(cg, TR::InstOpCode::ldrx, callNode, tmpReg, 0, secondClassCacheSlotLabel);
   generateCompareInstruction(cg, callNode, vftReg, tmpReg, true);
   TR::Instruction *gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, callNode, ifcSnippetLabel, TR::CC_NE);
   gcPoint->ARM64NeedsGCMap(cg, regMapForGC);
   TR::LabelSymbol *secondBranchAddressCacheSlotLabel = ifcSnippet->getSecondBranchAddressCacheSlotLabel();

   generateTrg1ImmSymInstruction(cg, TR::InstOpCode::ldrx, callNode, tmpReg, 0, secondBranchAddressCacheSlotLabel);
   generateLabelInstruction(cg, TR::InstOpCode::label, callNode, hitLabel);
   gcPoint = generateRegBranchInstruction(cg, TR::InstOpCode::blr, callNode, tmpReg);
   gcPoint->ARM64NeedsGCMap(cg, regMapForGC);
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

void J9::ARM64::PrivateLinkage::buildVirtualDispatch(TR::Node *callNode,
   TR::RegisterDependencyConditions *dependencies,
   uint32_t argSize)
   {
   TR::Register *x0 = dependencies->searchPreConditionRegister(TR::RealRegister::x0);
   TR::Register *x9 = dependencies->searchPreConditionRegister(TR::RealRegister::x9);

   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg());
   uint32_t regMapForGC = getProperties().getPreservedRegisterMapForGC();
   void *thunk = NULL;

   TR::Instruction *gcPoint;

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());

   // Computed calls
   //
   if (methodSymbol->isComputed())
      {
      TR::Register *vftReg = evaluateUpToVftChild(callNode, cg());
      TR::addDependency(dependencies, vftReg, getProperties().getComputedCallTargetRegister(), TR_GPR, cg());

      switch (methodSymbol->getMandatoryRecognizedMethod())
         {
         case TR::java_lang_invoke_ComputedCalls_dispatchVirtual:
         case TR::com_ibm_jit_JITHelpers_dispatchVirtual:
            {
            // Need a j2i thunk for the method that will ultimately be dispatched by this handle call
            char *j2iSignature = fej9->getJ2IThunkSignatureForDispatchVirtual(methodSymbol->getMethod()->signatureChars(), methodSymbol->getMethod()->signatureLength(), comp());
            int32_t signatureLen = strlen(j2iSignature);
            thunk = fej9->getJ2IThunk(j2iSignature, signatureLen, comp());
            if (!thunk)
               {
               thunk = fej9->setJ2IThunk(j2iSignature, signatureLen,
                                         TR::ARM64CallSnippet::generateVIThunk(fej9->getEquivalentVirtualCallNodeForDispatchVirtual(callNode, comp()), argSize, cg()), comp());
               }
            }
         default:
            if (fej9->needsInvokeExactJ2IThunk(callNode, comp()))
               {
               comp()->getPersistentInfo()->getInvokeExactJ2IThunkTable()->addThunk(
                  TR::ARM64CallSnippet::generateInvokeExactJ2IThunk(callNode, argSize, cg(), methodSymbol->getMethod()->signatureChars()), fej9);
               }
            break;
         }

      TR::Instruction *gcPoint = generateRegBranchInstruction(cg(), TR::InstOpCode::blr, callNode, vftReg, dependencies);
      gcPoint->ARM64NeedsGCMap(cg(), regMapForGC);

      return;
      }

   // Virtual and interface calls
   //
   TR_ASSERT_FATAL(methodSymbol->isVirtual() || methodSymbol->isInterface(), "Unexpected method type");

   thunk = fej9->getJ2IThunk(methodSymbol->getMethod(), comp());
   if (!thunk)
      thunk = fej9->setJ2IThunk(methodSymbol->getMethod(), TR::ARM64CallSnippet::generateVIThunk(callNode, argSize, cg()), comp());

   bool callIsSafe = methodSymRef != comp()->getSymRefTab()->findObjectNewInstanceImplSymbol();

   // evaluate vftReg because it is required for implicit NULLCHK
   TR::Register *vftReg = evaluateUpToVftChild(callNode, cg());
   TR::addDependency(dependencies, vftReg, TR::RealRegister::NoReg, TR_GPR, cg());

   if (methodSymbol->isVirtual())
      {
      TR::MemoryReference *tempMR;
      if (methodSymRef->isUnresolved() || comp()->compileRelocatableCode())
         {
         TR::LabelSymbol *vcSnippetLabel = generateLabelSymbol(cg());
         TR::ARM64VirtualUnresolvedSnippet *vcSnippet =
            new (trHeapMemory())
            TR::ARM64VirtualUnresolvedSnippet(cg(), callNode, vcSnippetLabel, argSize, doneLabel, (uint8_t *)thunk);
         cg()->addSnippet(vcSnippet);


         // The following instructions are modified by _virtualUnresolvedHelper
         // in aarch64/runtime/PicBuilder.spp to load the vTable index in x9

         // This `b` instruction is modified to movzx x9, lower 16bit of offset
         generateLabelInstruction(cg(), TR::InstOpCode::b, callNode, vcSnippetLabel);
         generateTrg1ImmInstruction(cg(), TR::InstOpCode::movkx, callNode, x9, TR::MOV_LSL16);
         generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::sbfmx, callNode, x9, x9, 0x1F); // sxtw x9, w9
         tempMR = TR::MemoryReference::createWithIndexReg(cg(), vftReg, x9);
         generateTrg1MemInstruction(cg(), TR::InstOpCode::ldroffx, callNode, x9, tempMR);
         gcPoint = generateRegBranchInstruction(cg(), TR::InstOpCode::blr, callNode, x9);
         gcPoint->ARM64NeedsGCMap(cg(), regMapForGC);
         generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, doneLabel, dependencies);

         return;
         }

      // Handle guarded devirtualization next
      //
      if (callIsSafe)
         {
         TR::ResolvedMethodSymbol *resolvedMethodSymbol = methodSymRef->getSymbol()->getResolvedMethodSymbol();
         TR_ResolvedMethod       *resolvedMethod = resolvedMethodSymbol->getResolvedMethod();

         if (comp()->performVirtualGuardNOPing() &&
             comp()->isVirtualGuardNOPingRequired() &&
             !resolvedMethod->isInterpreted() &&
             !callNode->isTheVirtualCallNodeForAGuardedInlinedCall())
            {
            TR_VirtualGuard *virtualGuard = NULL;

            if (!resolvedMethod->virtualMethodIsOverridden() &&
                !resolvedMethod->isAbstract())
               {
               if (comp()->getOption(TR_TraceCG))
                  {
                  traceMsg(comp(), "Creating TR_NonoverriddenGuard for node %p\n", callNode);
                  }
               virtualGuard = TR_VirtualGuard::createGuardedDevirtualizationGuard(TR_NonoverriddenGuard, comp(), callNode);
               }
            else
               {
               TR_DevirtualizedCallInfo *devirtualizedCallInfo = comp()->findDevirtualizedCall(callNode);
               TR_OpaqueClassBlock      *refinedThisClass = devirtualizedCallInfo ? devirtualizedCallInfo->_thisType : NULL;
               TR_OpaqueClassBlock      *thisClass = refinedThisClass ? refinedThisClass : resolvedMethod->containingClass();

               TR_PersistentCHTable     *chTable = comp()->getPersistentInfo()->getPersistentCHTable();
               /* Devirtualization is not currently supported for AOT compilations */
               if (thisClass && TR::Compiler->cls.isAbstractClass(comp(), thisClass) && !comp()->compileRelocatableCode())
                  {
                  TR_ResolvedMethod *calleeMethod = chTable->findSingleAbstractImplementer(thisClass, methodSymRef->getOffset(), methodSymRef->getOwningMethod(comp()), comp());
                  if (calleeMethod &&
                      (comp()->isRecursiveMethodTarget(calleeMethod) ||
                       !calleeMethod->isInterpreted() ||
                       calleeMethod->isJITInternalNative()))
                     {
                     if (comp()->getOption(TR_TraceCG))
                        {
                        traceMsg(comp(), "Creating TR_AbstractGuard for node %p\n", callNode);
                        }
                     resolvedMethod = calleeMethod;
                     virtualGuard = TR_VirtualGuard::createGuardedDevirtualizationGuard(TR_AbstractGuard, comp(), callNode);
                     }
                  }
               else if (refinedThisClass &&
                        resolvedMethod->virtualMethodIsOverridden() &&
                        !chTable->isOverriddenInThisHierarchy(resolvedMethod, refinedThisClass, methodSymRef->getOffset(), comp()))
                  {
                  TR_ResolvedMethod *calleeMethod = methodSymRef->getOwningMethod(comp())->getResolvedVirtualMethod(comp(), refinedThisClass, methodSymRef->getOffset());
                  if (calleeMethod &&
                      (comp()->isRecursiveMethodTarget(calleeMethod) ||
                       !calleeMethod->isInterpreted() ||
                       calleeMethod->isJITInternalNative()))
                     {
                     if (comp()->getOption(TR_TraceCG))
                        {
                        traceMsg(comp(), "Creating TR_HierarchyGuard for node %p\n", callNode);
                        }
                     resolvedMethod = calleeMethod;
                     virtualGuard = TR_VirtualGuard::createGuardedDevirtualizationGuard(TR_HierarchyGuard, comp(), callNode);
                     }
                  }
               }

            // If we have a virtual call guard generate a direct call
            // in the inline path and the virtual call out of line.
            // If the guard is later patched we'll go out of line path.
            //
            if (virtualGuard)
               {
               TR::LabelSymbol *virtualCallLabel = generateLabelSymbol(cg());
               generateVirtualGuardNOPInstruction(cg(), callNode, virtualGuard->addNOPSite(), NULL, virtualCallLabel);

               if (comp()->getOption(TR_EnableHCR))
                  {
                  if (cg()->supportsMergingGuards())
                     {
                     virtualGuard->setMergedWithHCRGuard();
                     }
                  else
                     {
                     TR_VirtualGuard *HCRGuard = TR_VirtualGuard::createGuardedDevirtualizationGuard(TR_HCRGuard, comp(), callNode);
                     generateVirtualGuardNOPInstruction(cg(), callNode, HCRGuard->addNOPSite(), NULL, virtualCallLabel);
                     }
                  }
               if (resolvedMethod != resolvedMethodSymbol->getResolvedMethod())
                  {
                  methodSymRef = comp()->getSymRefTab()->findOrCreateMethodSymbol(methodSymRef->getOwningMethodIndex(),
                                                                                  -1,
                                                                                  resolvedMethod,
                                                                                  TR::MethodSymbol::Virtual);
                  methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
                  resolvedMethodSymbol = methodSymbol->getResolvedMethodSymbol();
                  resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
                  }
               uintptr_t methodAddress = comp()->isRecursiveMethodTarget(resolvedMethod) ? 0 : (uintptr_t)resolvedMethod->startAddressForJittedMethod();
               TR::Instruction *gcPoint = generateImmSymInstruction(cg(), TR::InstOpCode::bl, callNode, methodAddress, NULL, methodSymRef, NULL);
               generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, doneLabel, dependencies);
               gcPoint->ARM64NeedsGCMap(cg(), regMapForGC);

               fej9->reserveTrampolineIfNecessary(comp(), methodSymRef, false);

               // Out of line virtual call
               //
               TR_ARM64OutOfLineCodeSection *virtualCallOOL = new (trHeapMemory()) TR_ARM64OutOfLineCodeSection(virtualCallLabel, doneLabel, cg());

               virtualCallOOL->swapInstructionListsWithCompilation();
               TR::Instruction *OOLLabelInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, virtualCallLabel);

               // XXX: Temporary fix, OOL instruction stream does not pick up live locals or monitors correctly.
               TR_ASSERT(!OOLLabelInstr->getLiveLocals() && !OOLLabelInstr->getLiveMonitors(), "Expecting first OOL instruction to not have live locals/monitors info");
               OOLLabelInstr->setLiveLocals(gcPoint->getLiveLocals());
               OOLLabelInstr->setLiveMonitors(gcPoint->getLiveMonitors());

               buildVirtualCall(cg(), callNode, vftReg, x9, regMapForGC);

               generateLabelInstruction(cg(), TR::InstOpCode::b, callNode, doneLabel);
               virtualCallOOL->swapInstructionListsWithCompilation();
               cg()->getARM64OutOfLineCodeSectionList().push_front(virtualCallOOL);

               return;
               }
            }
         }
      }

   // Profile-driven virtual and interface calls
   //
   // If the top value dominates everything else, generate a single static
   // PIC call inline and a virtual call or dynamic PIC call out of line.
   //
   // Otherwise generate a reasonable amount of static PIC calls and a
   // virtual call or dynamic PIC call all inline.
   //
   if (callIsSafe && !callNode->isTheVirtualCallNodeForAGuardedInlinedCall() && !comp()->getOption(TR_DisableInterpreterProfiling))
      {
      static uint32_t  maxVirtualStaticPICs = comp()->getOptions()->getMaxStaticPICSlots(comp()->getMethodHotness());
      static uint32_t  maxInterfaceStaticPICs = comp()->getOptions()->getNumInterfaceCallCacheSlots();

      TR_ScratchList<J9::ARM64PICItem> values(cg()->trMemory());
      const uint32_t maxStaticPICs = methodSymbol->isInterface() ? maxInterfaceStaticPICs : maxVirtualStaticPICs;

      if (getProfiledCallSiteInfo(cg(), callNode, maxStaticPICs, values))
         {
         ListIterator<J9::ARM64PICItem>  i(&values);
         J9::ARM64PICItem               *pic = i.getFirst();

         // If this value is dominant, optimize exclusively for it
         if (pic->_frequency > MAX_PROFILED_CALL_FREQUENCY)
            {
            if (comp()->getOption(TR_TraceCG))
               {
               traceMsg(comp(), "Found dominant profiled target, frequency = %f\n", pic->_frequency);
               }
            TR::LabelSymbol *slowCallLabel = generateLabelSymbol(cg());

            TR::Instruction *gcPoint = buildStaticPICCall(cg(), callNode, pic->_clazz, pic->_method,
                                                            vftReg, x9, slowCallLabel, regMapForGC);
            generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, doneLabel, dependencies);

            // Out of line virtual/interface call
            //
            TR_ARM64OutOfLineCodeSection *slowCallOOL = new (trHeapMemory()) TR_ARM64OutOfLineCodeSection(slowCallLabel, doneLabel, cg());

            slowCallOOL->swapInstructionListsWithCompilation();
            TR::Instruction *OOLLabelInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, slowCallLabel);

            // XXX: Temporary fix, OOL instruction stream does not pick up live locals or monitors correctly.
            TR_ASSERT_FATAL(!OOLLabelInstr->getLiveLocals() && !OOLLabelInstr->getLiveMonitors(), "Expecting first OOL instruction to not have live locals/monitors info");
            OOLLabelInstr->setLiveLocals(gcPoint->getLiveLocals());
            OOLLabelInstr->setLiveMonitors(gcPoint->getLiveMonitors());

            TR::LabelSymbol *doneOOLLabel = generateLabelSymbol(cg());

            if (methodSymbol->isInterface())
               {
               TR::LabelSymbol               *ifcSnippetLabel = generateLabelSymbol(cg());
               TR::LabelSymbol               *firstClassCacheSlotLabel = generateLabelSymbol(cg());
               TR::LabelSymbol               *firstBranchAddressCacheSlotLabel = generateLabelSymbol(cg());
               TR::LabelSymbol               *secondClassCacheSlotLabel = generateLabelSymbol(cg());
               TR::LabelSymbol               *secondBranchAddressCacheSlotLabel = generateLabelSymbol(cg());
               TR::ARM64InterfaceCallSnippet *ifcSnippet = new (trHeapMemory()) TR::ARM64InterfaceCallSnippet(cg(), callNode, ifcSnippetLabel,
                                                                                                              argSize, doneOOLLabel, firstClassCacheSlotLabel, secondClassCacheSlotLabel,
                                                                                                              firstBranchAddressCacheSlotLabel, secondBranchAddressCacheSlotLabel, static_cast<uint8_t *>(thunk));
               cg()->addSnippet(ifcSnippet);
               buildInterfaceCall(cg(), callNode, vftReg, x9, ifcSnippet, regMapForGC);
               }
            else
               {
               buildVirtualCall(cg(), callNode, vftReg, x9, regMapForGC);
               }

            generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, doneOOLLabel);
            generateLabelInstruction(cg(), TR::InstOpCode::b, callNode, doneLabel);
            slowCallOOL->swapInstructionListsWithCompilation();
            cg()->getARM64OutOfLineCodeSectionList().push_front(slowCallOOL);

            return;
            }
         else
            {
             if (comp()->getOption(TR_TraceCG))
               {
               traceMsg(comp(), "Generating %d static PIC calls\n", values.getSize());
               }
            // Build multiple static PIC calls
            while (pic)
               {
               TR::LabelSymbol *nextLabel = generateLabelSymbol(cg());

               buildStaticPICCall(cg(), callNode, pic->_clazz, pic->_method,
                                  vftReg, x9, nextLabel, regMapForGC);
               generateLabelInstruction(cg(), TR::InstOpCode::b, callNode, doneLabel);
               generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, nextLabel);
               pic = i.getNext();
               }
            // Regular virtual/interface call will be built below
            }
         }
      }

   // Finally, regular virtual and interface calls
   //
   if (methodSymbol->isInterface())
      {
      // interface calls
      // ToDo: Inline interface dispatch

      TR::LabelSymbol *ifcSnippetLabel = generateLabelSymbol(cg());
      TR::LabelSymbol *firstClassCacheSlotLabel = generateLabelSymbol(cg());
      TR::LabelSymbol *firstBranchAddressCacheSlotLabel = generateLabelSymbol(cg());
      TR::LabelSymbol *secondClassCacheSlotLabel = generateLabelSymbol(cg());
      TR::LabelSymbol *secondBranchAddressCacheSlotLabel = generateLabelSymbol(cg());
      TR::ARM64InterfaceCallSnippet *ifcSnippet =
         new (trHeapMemory())
         TR::ARM64InterfaceCallSnippet(cg(), callNode, ifcSnippetLabel, argSize, doneLabel, firstClassCacheSlotLabel, firstBranchAddressCacheSlotLabel, secondClassCacheSlotLabel, secondBranchAddressCacheSlotLabel, static_cast<uint8_t *>(thunk));
      cg()->addSnippet(ifcSnippet);

      buildInterfaceCall(cg(), callNode, vftReg, x9, ifcSnippet, regMapForGC);
      }
   else
      {
      buildVirtualCall(cg(), callNode, vftReg, x9, regMapForGC);
      }
   generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, doneLabel, dependencies);
   }

TR::Register *J9::ARM64::PrivateLinkage::buildIndirectDispatch(TR::Node *callNode)
   {
   const TR::ARM64LinkageProperties &pp = getProperties();
   TR::RealRegister *sp = cg()->machine()->getRealRegister(pp.getStackPointerRegister());

   // Extra post dependency for killing vector registers (see KillVectorRegs)
   const int extraPostReg = killsVectorRegisters() ? 1 : 0;
   TR::RegisterDependencyConditions *dependencies =
      new (trHeapMemory()) TR::RegisterDependencyConditions(
         pp.getNumberOfDependencyGPRegisters(),
         pp.getNumberOfDependencyGPRegisters() + extraPostReg, trMemory());

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

   dependencies->stopUsingDepRegs(cg(), retReg);
   return retReg;
   }

TR::Instruction *
J9::ARM64::PrivateLinkage::loadStackParametersToLinkageRegisters(TR::Instruction *cursor)
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
         TR::DataType dataType = parmCursor->getDataType();

         if (dataType == TR::Double || dataType == TR::Float)
            {
            linkageReg = machine->getRealRegister(properties.getFloatArgumentRegister(lri));
            op = (dataType == TR::Double) ? TR::InstOpCode::vldrimmd : TR::InstOpCode::vldrimms;
            }
         else
            {
            linkageReg = machine->getRealRegister(properties.getIntegerArgumentRegister(lri));
            op = (dataType == TR::Int64 || dataType == TR::Address) ? TR::InstOpCode::ldrimmx : TR::InstOpCode::ldrimmw;
            }

         TR::MemoryReference *stackMR = TR::MemoryReference::createWithDisplacement(cg(), javaSP, parmCursor->getParameterOffset());
         cursor = generateTrg1MemInstruction(cg(), op, NULL, linkageReg, stackMR, cursor);
         }
      }

    return cursor;
    }

TR::Instruction *
J9::ARM64::PrivateLinkage::saveParametersToStack(TR::Instruction *cursor)
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

         TR::MemoryReference *stackMR = TR::MemoryReference::createWithDisplacement(cg(), javaSP, parmCursor->getParameterOffset());
         cursor = generateMemSrc1Instruction(cg(), op, NULL, stackMR, linkageReg, cursor);
         }
      }

   return cursor;
   }

void J9::ARM64::PrivateLinkage::performPostBinaryEncoding()
   {
   // --------------------------------------------------------------------------
   // Encode the size of the interpreter entry area into the linkage info word
   //
   TR_ASSERT_FATAL(cg()->getReturnTypeInfoInstruction(),
                   "Expecting the return type info instruction to be created");

   TR::ARM64ImmInstruction *linkageInfoWordInstruction = cg()->getReturnTypeInfoInstruction();
   uint32_t linkageInfoWord = linkageInfoWordInstruction->getSourceImmediate();

   intptr_t jittedMethodEntryAddress = reinterpret_cast<intptr_t>(getJittedMethodEntryPoint()->getBinaryEncoding());
   intptr_t interpretedMethodEntryAddress = reinterpret_cast<intptr_t>(getInterpretedMethodEntryPoint()->getBinaryEncoding());

   linkageInfoWord = (static_cast<uint32_t>(jittedMethodEntryAddress - interpretedMethodEntryAddress) << 16) | linkageInfoWord;
   linkageInfoWordInstruction->setSourceImmediate(linkageInfoWord);

   *(uint32_t *)(linkageInfoWordInstruction->getBinaryEncoding()) = linkageInfoWord;

   // Set recompilation info
   //
   TR::Recompilation *recomp = comp()->getRecompilationInfo();
   if (recomp != NULL && recomp->couldBeCompiledAgain())
      {
      J9::PrivateLinkage::LinkageInfo *lkInfo = J9::PrivateLinkage::LinkageInfo::get(cg()->getCodeStart());
      if (recomp->useSampling())
         lkInfo->setSamplingMethodBody();
      else
         lkInfo->setCountingMethodBody();
      }
   }

int32_t J9::ARM64::HelperLinkage::buildArgs(TR::Node *callNode,
   TR::RegisterDependencyConditions *dependencies)
   {
   return buildPrivateLinkageArgs(callNode, dependencies, _helperLinkage);
   }
