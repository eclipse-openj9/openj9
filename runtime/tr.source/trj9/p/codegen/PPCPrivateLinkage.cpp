/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#include "p/codegen/PPCPrivateLinkage.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/VirtualGuard.hpp"
#include "env/CHTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/J2IThunk.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/VMJ9.h"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "p/codegen/CallSnippet.hpp"
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/PPCEvaluator.hpp"
#include "p/codegen/PPCHelperCallSnippet.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/PPCTableOfConstants.hpp"
#include "p/codegen/StackCheckFailureSnippet.hpp"
#include "runtime/J9Profiler.hpp"
#include "runtime/J9ValueProfiler.hpp"

#define MIN_PROFILED_CALL_FREQUENCY (.075f)
#define MAX_PROFILED_CALL_FREQUENCY (.90f)

TR::PPCPrivateLinkage::PPCPrivateLinkage(TR::CodeGenerator *cg)
   : TR::Linkage(cg)
   {
   int i = 0;
   bool is32bitLinux = false;

   _properties._properties = 0;
   _properties._registerFlags[TR::RealRegister::NoReg] = 0;
   _properties._registerFlags[TR::RealRegister::gr0]   = 0;
   _properties._registerFlags[TR::RealRegister::gr1]   = Preserved|PPC_Reserved; // system sp

   // gr2 is Preserved in 32-bit Linux
   if (TR::Compiler->target.is64Bit())
      {
      _properties._registerFlags[TR::RealRegister::gr2]   = 0;
      }
   else
      {
      if (TR::Compiler->target.isAIX())
         {
         _properties._registerFlags[TR::RealRegister::gr2] = 0;
         }
      else if (TR::Compiler->target.isLinux())
         {
         _properties._registerFlags[TR::RealRegister::gr2] = Preserved|PPC_Reserved;
         is32bitLinux = true;
         }
      else
         {
         TR_ASSERT(0, "unsupported target");
         }
      }

   _properties._registerFlags[TR::RealRegister::gr3]  = IntegerReturn|IntegerArgument;

   if (TR::Compiler->target.is64Bit())
      _properties._registerFlags[TR::RealRegister::gr4]  = IntegerArgument;
   else
      _properties._registerFlags[TR::RealRegister::gr4]  = IntegerReturn|IntegerArgument;

   _properties._registerFlags[TR::RealRegister::gr5]  = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::gr6]  = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::gr7]  = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::gr8]  = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::gr9]  = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::gr10] = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::gr11] = 0;
   _properties._registerFlags[TR::RealRegister::gr12] = 0;
   _properties._registerFlags[TR::RealRegister::gr13] = Preserved|PPC_Reserved; // meta data for 32bit, system for 64bit;
   _properties._registerFlags[TR::RealRegister::gr14] = Preserved|PPC_Reserved; // J9 sp
   if (TR::Compiler->target.is64Bit())
      {
      _properties._registerFlags[TR::RealRegister::gr15] = Preserved|PPC_Reserved; // meta data
      _properties._registerFlags[TR::RealRegister::gr16] = Preserved|PPC_Reserved; // JTOC
      }
   else
      {
      _properties._registerFlags[TR::RealRegister::gr15] = Preserved;
      _properties._registerFlags[TR::RealRegister::gr16] = Preserved;
      }

   for (i = TR::RealRegister::gr17; i <= TR::RealRegister::LastGPR; i++)
      _properties._registerFlags[i]  = Preserved; // gr17 - gr31 preserved

   _properties._registerFlags[TR::RealRegister::fp0]   = FloatReturn|FloatArgument;
   _properties._registerFlags[TR::RealRegister::fp1]   = FloatArgument;
   _properties._registerFlags[TR::RealRegister::fp2]   = FloatArgument;
   _properties._registerFlags[TR::RealRegister::fp3]   = FloatArgument;
   _properties._registerFlags[TR::RealRegister::fp4]   = FloatArgument;
   _properties._registerFlags[TR::RealRegister::fp5]   = FloatArgument;
   _properties._registerFlags[TR::RealRegister::fp6]   = FloatArgument;
   _properties._registerFlags[TR::RealRegister::fp7]   = FloatArgument;

   for (i = TR::RealRegister::fp8; i <= TR::RealRegister::LastFPR; i++)
      _properties._registerFlags[i]  = 0; // fp8 - fp31 volatile

   for (i = TR::RealRegister::vsr32; i <= TR::RealRegister::LastVSR; i++)
      _properties._registerFlags[i]  = 0; // vsr32 - vsr63 volatile

   for (i = TR::RealRegister::FirstCCR; i <= TR::RealRegister::LastCCR; i++)
      _properties._registerFlags[i]  = 0; // cr0 - cr7 volatile

   _properties._numIntegerArgumentRegisters  = 8;
   _properties._firstIntegerArgumentRegister = 0;
   _properties._numFloatArgumentRegisters    = 8;
   _properties._firstFloatArgumentRegister   = 8;

   _properties._argumentRegisters[0]  = TR::RealRegister::gr3;
   _properties._argumentRegisters[1]  = TR::RealRegister::gr4;
   _properties._argumentRegisters[2]  = TR::RealRegister::gr5;
   _properties._argumentRegisters[3]  = TR::RealRegister::gr6;
   _properties._argumentRegisters[4]  = TR::RealRegister::gr7;
   _properties._argumentRegisters[5]  = TR::RealRegister::gr8;
   _properties._argumentRegisters[6]  = TR::RealRegister::gr9;
   _properties._argumentRegisters[7]  = TR::RealRegister::gr10;
   _properties._argumentRegisters[8]  = TR::RealRegister::fp0;
   _properties._argumentRegisters[9]  = TR::RealRegister::fp1;
   _properties._argumentRegisters[10] = TR::RealRegister::fp2;
   _properties._argumentRegisters[11] = TR::RealRegister::fp3;
   _properties._argumentRegisters[12] = TR::RealRegister::fp4;
   _properties._argumentRegisters[13] = TR::RealRegister::fp5;
   _properties._argumentRegisters[14] = TR::RealRegister::fp6;
   _properties._argumentRegisters[15] = TR::RealRegister::fp7;

   _properties._firstIntegerReturnRegister = 0;
   _properties._returnRegisters[0] = TR::RealRegister::gr3;

   if (TR::Compiler->target.is64Bit())
      {
      _properties._firstFloatReturnRegister = 1;
      _properties._returnRegisters[1]  = TR::RealRegister::fp0;

      _properties._numAllocatableIntegerRegisters          = 27; // 64
      _properties._firstAllocatableFloatArgumentRegister   = 40; // 64
      _properties._lastAllocatableFloatVolatileRegister    = 58; // 64
      }
   else
      {
      _properties._firstFloatReturnRegister = 2;
      _properties._returnRegisters[1]  = TR::RealRegister::gr4;
      _properties._returnRegisters[2]  = TR::RealRegister::fp0;

      if (is32bitLinux)
         {
         _properties._numAllocatableIntegerRegisters          = 28; // 32 linux
         _properties._firstAllocatableFloatArgumentRegister   = 41; // 32 linux
         _properties._lastAllocatableFloatVolatileRegister    = 59; // 32 linux
         }
      else
         {
         _properties._numAllocatableIntegerRegisters          = 29; // 32 non-linux
         _properties._firstAllocatableFloatArgumentRegister   = 42; // 32 non-linux
         _properties._lastAllocatableFloatVolatileRegister    = 60; // 32 non-linux
         }
      }

   if (is32bitLinux)
      {
      _properties._firstAllocatableIntegerArgumentRegister = 8;
      _properties._lastAllocatableIntegerVolatileRegister  = 10;
      }
   else
      {
      _properties._firstAllocatableIntegerArgumentRegister = 9;
      _properties._lastAllocatableIntegerVolatileRegister  = 11;
      }
   _properties._numAllocatableFloatRegisters            = 32;
   _properties._numAllocatableVectorRegisters           = 32;
   _properties._numAllocatableCCRegisters               = 8;

   i = 0;
   if (!is32bitLinux)
      _properties._allocationOrder[i++] = TR::RealRegister::gr2;
   _properties._allocationOrder[i++] = TR::RealRegister::gr12;
   _properties._allocationOrder[i++] = TR::RealRegister::gr10;
   _properties._allocationOrder[i++] = TR::RealRegister::gr9;
   _properties._allocationOrder[i++] = TR::RealRegister::gr8;
   _properties._allocationOrder[i++] = TR::RealRegister::gr7;
   _properties._allocationOrder[i++] = TR::RealRegister::gr6;
   _properties._allocationOrder[i++] = TR::RealRegister::gr5;
   _properties._allocationOrder[i++] = TR::RealRegister::gr4;
   _properties._allocationOrder[i++] = TR::RealRegister::gr3;
   _properties._allocationOrder[i++] = TR::RealRegister::gr11;
   _properties._allocationOrder[i++] = TR::RealRegister::gr0;
   _properties._allocationOrder[i++] = TR::RealRegister::gr31;
   _properties._allocationOrder[i++] = TR::RealRegister::gr30;
   _properties._allocationOrder[i++] = TR::RealRegister::gr29;
   _properties._allocationOrder[i++] = TR::RealRegister::gr28;
   _properties._allocationOrder[i++] = TR::RealRegister::gr27;
   _properties._allocationOrder[i++] = TR::RealRegister::gr26;
   _properties._allocationOrder[i++] = TR::RealRegister::gr25;
   _properties._allocationOrder[i++] = TR::RealRegister::gr24;
   _properties._allocationOrder[i++] = TR::RealRegister::gr23;
   _properties._allocationOrder[i++] = TR::RealRegister::gr22;
   _properties._allocationOrder[i++] = TR::RealRegister::gr21;
   _properties._allocationOrder[i++] = TR::RealRegister::gr20;
   _properties._allocationOrder[i++] = TR::RealRegister::gr19;
   _properties._allocationOrder[i++] = TR::RealRegister::gr18;
   _properties._allocationOrder[i++] = TR::RealRegister::gr17;

   if (TR::Compiler->target.is32Bit())
      {
      _properties._allocationOrder[i++] = TR::RealRegister::gr16;
      _properties._allocationOrder[i++] = TR::RealRegister::gr15;
      }

   _properties._allocationOrder[i++] = TR::RealRegister::fp13;
   _properties._allocationOrder[i++] = TR::RealRegister::fp12;
   _properties._allocationOrder[i++] = TR::RealRegister::fp11;
   _properties._allocationOrder[i++] = TR::RealRegister::fp10;
   _properties._allocationOrder[i++] = TR::RealRegister::fp9;
   _properties._allocationOrder[i++] = TR::RealRegister::fp8;
   _properties._allocationOrder[i++] = TR::RealRegister::fp7;
   _properties._allocationOrder[i++] = TR::RealRegister::fp6;
   _properties._allocationOrder[i++] = TR::RealRegister::fp5;
   _properties._allocationOrder[i++] = TR::RealRegister::fp4;
   _properties._allocationOrder[i++] = TR::RealRegister::fp3;
   _properties._allocationOrder[i++] = TR::RealRegister::fp2;
   _properties._allocationOrder[i++] = TR::RealRegister::fp1;
   _properties._allocationOrder[i++] = TR::RealRegister::fp0;
   _properties._allocationOrder[i++] = TR::RealRegister::fp31;
   _properties._allocationOrder[i++] = TR::RealRegister::fp30;
   _properties._allocationOrder[i++] = TR::RealRegister::fp29;
   _properties._allocationOrder[i++] = TR::RealRegister::fp28;
   _properties._allocationOrder[i++] = TR::RealRegister::fp27;
   _properties._allocationOrder[i++] = TR::RealRegister::fp26;
   _properties._allocationOrder[i++] = TR::RealRegister::fp25;
   _properties._allocationOrder[i++] = TR::RealRegister::fp24;
   _properties._allocationOrder[i++] = TR::RealRegister::fp23;
   _properties._allocationOrder[i++] = TR::RealRegister::fp22;
   _properties._allocationOrder[i++] = TR::RealRegister::fp21;
   _properties._allocationOrder[i++] = TR::RealRegister::fp20;
   _properties._allocationOrder[i++] = TR::RealRegister::fp19;
   _properties._allocationOrder[i++] = TR::RealRegister::fp18;
   _properties._allocationOrder[i++] = TR::RealRegister::fp17;
   _properties._allocationOrder[i++] = TR::RealRegister::fp16;
   _properties._allocationOrder[i++] = TR::RealRegister::fp15;
   _properties._allocationOrder[i++] = TR::RealRegister::fp14;

   _properties._allocationOrder[i++] = TR::RealRegister::vsr32;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr33;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr34;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr35;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr36;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr37;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr38;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr39;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr40;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr41;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr42;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr43;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr44;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr45;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr46;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr47;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr48;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr49;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr50;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr51;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr52;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr53;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr54;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr55;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr56;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr57;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr58;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr59;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr60;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr61;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr62;
   _properties._allocationOrder[i++] = TR::RealRegister::vsr63;

   _properties._allocationOrder[i++] = TR::RealRegister::cr7;
   _properties._allocationOrder[i++] = TR::RealRegister::cr6;
   _properties._allocationOrder[i++] = TR::RealRegister::cr5;
   _properties._allocationOrder[i++] = TR::RealRegister::cr1;
   _properties._allocationOrder[i++] = TR::RealRegister::cr0;
   _properties._allocationOrder[i++] = TR::RealRegister::cr2;
   _properties._allocationOrder[i++] = TR::RealRegister::cr3;
   _properties._allocationOrder[i++] = TR::RealRegister::cr4;

   if (TR::Compiler->target.is64Bit())
      {
      _properties._preservedRegisterMapForGC     = 0x00007fff;
      _properties._methodMetaDataRegister        = TR::RealRegister::gr15;
      _properties._normalStackPointerRegister    = TR::RealRegister::gr14;
      _properties._alternateStackPointerRegister = TR::RealRegister::NoReg;
      _properties._TOCBaseRegister               = TR::RealRegister::gr16;
      // Volatile GPR (0,2-12) + FPR (0-31) + CCR (0-7) + VR (0-31)
      _properties._numberOfDependencyGPRegisters = 12 + 32 + 8 + 32;
      _properties._offsetToFirstParm             = 0;
      _properties._offsetToFirstLocal            = -8;
      }
   else
      {
      _properties._preservedRegisterMapForGC     = 0x0001ffff;
      _properties._methodMetaDataRegister        = TR::RealRegister::gr13;
      _properties._normalStackPointerRegister    = TR::RealRegister::gr14;
      _properties._alternateStackPointerRegister = TR::RealRegister::NoReg;
      _properties._TOCBaseRegister               = TR::RealRegister::NoReg;
      if (is32bitLinux)
         // Volatile GPR (0,3-12) + FPR (0-31) + CCR (0-7) + VR (0-31)
         _properties._numberOfDependencyGPRegisters = 11 + 32 + 8 + 32;
      else
         // Volatile GPR (0,2-12) + FPR (0-31) + CCR (0-7) + VR (0-31)
         _properties._numberOfDependencyGPRegisters = 12 + 32 + 8 + 32;
      _properties._offsetToFirstParm             = 0;
      _properties._offsetToFirstLocal            = -4;
      }
   _properties._computedCallTargetRegister  = TR::RealRegister::gr0; // gr11 = interface, gr12 = virtual, so we need something else for computed
   _properties._vtableIndexArgumentRegister = TR::RealRegister::gr12;
   _properties._j9methodArgumentRegister    = TR::RealRegister::gr3; // TODO:JSR292: Confirm

   // shrink wrapping
   setRegisterSaveSize(0);
   _mapRegsToStack = (int32_t *) trMemory()->allocateHeapMemory(TR::RealRegister::NumRegisters*sizeof(int32_t));
   memset(_mapRegsToStack, -1, (TR::RealRegister::NumRegisters*sizeof(int32_t)));
   }

const TR::PPCLinkageProperties& TR::PPCPrivateLinkage::getProperties()
   {
   return _properties;
   }

void TR::PPCPrivateLinkage::initPPCRealRegisterLinkage()
   {
   TR::Machine *machine = cg()->machine();
   const TR::PPCLinkageProperties &linkage = getProperties();
   int icount, ret_count=0, lockedGPRs = 0;

   for (icount=TR::RealRegister::FirstGPR; icount<=TR::RealRegister::gr12;
        icount++)
      {
      if (linkage.getReserved((TR::RealRegister::RegNum)icount))
         {
         machine->getPPCRealRegister((TR::RealRegister::RegNum)icount)->setState(TR::RealRegister::Locked);
         ++lockedGPRs;
         machine->getPPCRealRegister((TR::RealRegister::RegNum)icount)->setAssignedRegister(machine->getPPCRealRegister((TR::RealRegister::RegNum)icount));
         }
      else
         {
         int  weight;
         if (linkage.getIntegerReturn((TR::RealRegister::RegNum)icount))
            {
            weight = ++ret_count;
            }
         else
            {
            if (icount < TR::RealRegister::gr3)
               {
               weight = 2 + icount;
               }
            else
               {
               weight = icount;
               }
            }
         machine->getPPCRealRegister((TR::RealRegister::RegNum)icount)->setWeight(weight);
         }
      }

   for (icount=TR::RealRegister::LastGPR;
        icount>=TR::RealRegister::gr13; icount--)
      {
      if (linkage.getReserved((TR::RealRegister::RegNum)icount))
         {
         machine->getPPCRealRegister((TR::RealRegister::RegNum)icount)->setState(TR::RealRegister::Locked);
         ++lockedGPRs;
         machine->getPPCRealRegister((TR::RealRegister::RegNum)icount)->setAssignedRegister(machine->getPPCRealRegister((TR::RealRegister::RegNum)icount));
         }
      machine->getPPCRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000-icount);
      }

   int lowestFPRWeight = TR::RealRegister::FirstFPR;

   for (icount=TR::RealRegister::FirstFPR;
        icount<=TR::RealRegister::LastFPR; icount++)
      {
      if (linkage.getPreserved((TR::RealRegister::RegNum)icount))
         {
         machine->getPPCRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000-icount);
         }
      else
         {
         machine->getPPCRealRegister((TR::RealRegister::RegNum)icount)->setWeight(lowestFPRWeight);
         }
      }

   for (icount=TR::RealRegister::FirstVRF;
        icount<=TR::RealRegister::LastVRF; icount++)
      {
      if (linkage.getPreserved((TR::RealRegister::RegNum)icount))
         {
         machine->getPPCRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000-icount);
         }
      else
         {
         machine->getPPCRealRegister((TR::RealRegister::RegNum)icount)->setWeight(icount);
         }
      }

   for (icount=TR::RealRegister::FirstCCR;
        icount<=TR::RealRegister::LastCCR; icount++)
      {
      if (linkage.getPreserved((TR::RealRegister::RegNum)icount))
         {
         machine->getPPCRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000-icount);
         }
      else
         {
         machine->getPPCRealRegister((TR::RealRegister::RegNum)icount)->setWeight(icount);
         }
      }

   machine->setNumberOfLockedRegisters(TR_GPR, lockedGPRs);
   machine->setNumberOfLockedRegisters(TR_FPR, 0);
   machine->setNumberOfLockedRegisters(TR_VRF, 0);
   }

uint32_t TR::PPCPrivateLinkage::getRightToLeft()
   {
   return getProperties().getRightToLeft();
   }

void TR::PPCPrivateLinkage::mapStack(TR::ResolvedMethodSymbol *method)
   {
   ListIterator<TR::AutomaticSymbol>  automaticIterator(&method->getAutomaticList());
   TR::AutomaticSymbol               *localCursor       = automaticIterator.getFirst();
   const TR::PPCLinkageProperties&    linkage           = getProperties();
   TR::Machine *machine = cg()->machine();
   TR::RealRegister::RegNum regIndex;
   int32_t                           firstLocalOffset  = linkage.getOffsetToFirstLocal();
   uint32_t                          stackIndex        = firstLocalOffset;
   int32_t                           lowGCOffset        = stackIndex;
   TR::GCStackAtlas                  *atlas             = cg()->getStackAtlas();
   int32_t firstLocalGCIndex = atlas->getNumberOfParmSlotsMapped();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());

   // map all garbage collected references together so can concisely represent
   // stack maps. They must be mapped so that the GC map index in each local
   // symbol is honoured.

   uint32_t numberOfLocalSlotsMapped = atlas->getNumberOfSlotsMapped() - atlas->getNumberOfParmSlotsMapped();

   stackIndex -= numberOfLocalSlotsMapped * TR::Compiler->om.sizeofReferenceAddress();

   if (comp()->useCompressedPointers())
   {
        // If we have any local objects we have to make sure they're aligned properly when compressed pointers are used,
        // otherwise pointer compression may clobber part of the pointer.
        // Each auto's GC index will have already been aligned, we just need to make sure
        // we align the starting stack offset.
        uint32_t unalignedStackIndex = stackIndex;
        stackIndex &= ~(fej9->getObjectAlignmentInBytes() - 1);
        uint32_t paddingBytes = unalignedStackIndex - stackIndex;
        if (paddingBytes > 0)
        {
            TR_ASSERT((paddingBytes & (TR::Compiler->om.sizeofReferenceAddress() - 1)) == 0, "Padding bytes should be a multiple of the slot/pointer size");
            uint32_t paddingSlots = paddingBytes / TR::Compiler->om.sizeofReferenceAddress();
            atlas->setNumberOfSlotsMapped(atlas->getNumberOfSlotsMapped() + paddingSlots);
        }
   }


   // Map local references again to set the stack position correct according to
   // the GC map index.
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
      if (TR::Compiler->target.is64Bit())
         {
         if (localCursor->getGCMapIndex() < 0 &&
             localCursor->getSize() != 8)
            {
            mapSingleAutomatic(localCursor, stackIndex);
            }
         }
      else
         {
         if (localCursor->getGCMapIndex() < 0 &&
             localCursor->getDataType() != TR::Double)
            {
            mapSingleAutomatic(localCursor, stackIndex);
            }
         }
      localCursor = automaticIterator.getNext();
      }

   automaticIterator.reset();
   localCursor = automaticIterator.getFirst();

   while (localCursor != NULL)
      {
      if (TR::Compiler->target.is64Bit())
         {
         if (localCursor->getGCMapIndex() < 0 &&
             localCursor->getSize() == 8)
            {
            stackIndex -= (stackIndex & 0x4)?4:0;
            mapSingleAutomatic(localCursor, stackIndex);
            }
         }
      else
         {
         if (localCursor->getGCMapIndex() < 0 &&
             localCursor->getDataType() == TR::Double)
            {
            stackIndex -= (stackIndex & 0x4)?4:0;
            mapSingleAutomatic(localCursor, stackIndex);
            }
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
      uint32_t sizeOfParameterArea = method->getNumParameterSlots() * TR::Compiler->om.sizeofReferenceAddress();
      while (parmCursor != NULL)
         {
         if (TR::Compiler->target.is64Bit() &&
             parmCursor->getDataType() != TR::Address)
            parmCursor->setParameterOffset(sizeOfParameterArea -
                                        parmCursor->getParameterOffset() -
                                        parmCursor->getSize()*2 +
                                        offsetToFirstParm);
         else
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

void TR::PPCPrivateLinkage::mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   int32_t roundup = (comp()->useCompressedPointers() && p->isLocalObject() ? fej9->getObjectAlignmentInBytes() : TR::Compiler->om.sizeofReferenceAddress()) - 1;
   int32_t roundedSize = (p->getSize() + roundup) & (~roundup);
   if (roundedSize == 0)
      roundedSize = 4;

   p->setOffset(stackIndex -= roundedSize);
   }

void TR::PPCPrivateLinkage::setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method)
   {
   ListIterator<TR::ParameterSymbol>   paramIterator(&(method->getParameterList()));
   TR::ParameterSymbol      *paramCursor = paramIterator.getFirst();
   int32_t                  numIntArgs = 0, numFloatArgs = 0;
   const TR::PPCLinkageProperties& properties = getProperties();

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
         case TR::Address:
            if (numIntArgs<properties.getNumIntArgRegs())
               {
               index = numIntArgs;
               }
            numIntArgs++;
            break;
         case TR::Int64:
            if (numIntArgs<properties.getNumIntArgRegs())
               {
               index = numIntArgs;
               }
            if (TR::Compiler->target.is64Bit())
               numIntArgs ++;
            else
               numIntArgs += 2;
            break;
         case TR::Float:
         case TR::Double:
            if (numFloatArgs<properties.getNumFloatArgRegs())
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

bool TR::PPCPrivateLinkage::hasToBeOnStack(TR::ParameterSymbol *parm)
   {
   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   TR_OpaqueClassBlock *throwableClass;

   /* Defect 138664: All synchronized methods can potentially throw an
      IllegalMonitorState exception so we must always save the this pointer on
      to the stack so the exception handler can unlock the object. Also,
      hasCall() does not consider jitNewObject() calls so an OOM exception could
      be thrown even when hasCall() returns false */

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());

   bool result = (parm->getAllocatedIndex()>=0       &&
          ( (  parm->getLinkageRegisterIndex()==0 &&
               parm->isCollectedReference()       &&
               !bodySymbol->isStatic()            &&
               (  (  bodySymbol->isSynchronised()
                  ) ||
                  (
                  !strncmp(bodySymbol->getResolvedMethod()->nameChars(), "<init>", 6) &&
                    ( (throwableClass = fej9->getClassFromSignature("Ljava/lang/Throwable;", 21, bodySymbol->getResolvedMethod())) == 0 ||
                      fej9->isInstanceOf(bodySymbol->getResolvedMethod()->containingClass(), throwableClass, true) != TR_no
                    )
                  )
               )
            ) ||
            parm->isParmHasToBeOnStack()
          )
         );

   // Problem Report 96788:
   //
   // There is a potential race condition here. Because of the query to the frontend this function could
   // possibly return different results at different points in the compilation dependent on whether the
   // java/lang/Throwable class is resolved or not. This is a problem because this query is used to
   // determine whether we need to generate a GC map for this parameter and whether we need to generate
   // a store out to the stack for this parameter. Because these two queries happen at two different points
   // in the compilation we could encounter a situation where we generate a GC map for this parameter but
   // not generate a store out to the stack. This causes assertions in the VM if we hit a GC point in this
   // compilation unit. To avoid this issue we cache the result of this function and directly modify the
   // parameter symbol.

   // TODO : Where does the java/lang/Throwable code below originate and why is it here? This seems like
   // a very hacky fix to a very specific problem. Also why is this code not commoned up with Z and why
   // is it missing for X?

   if (result)
      parm->setParmHasToBeOnStack();

   return result;
   }

static TR::Instruction *unrollPrologueInitLoop(TR::CodeGenerator *cg, TR::Node *node, int32_t num, int32_t initSlotOffset, TR::Register *nullReg,
   TR::RealRegister *gr11, TR::RealRegister *baseInitReg, TR::RealRegister *gr12, TR::RealRegister *cr0, TR::LabelSymbol *loopLabel,
   TR::Instruction *cursor)
   {
   TR_HeapMemory trHeapMemory    = cg->trMemory();
   int32_t wordsToUnroll         = num;
   TR::Compilation* comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

   static bool disableVSXMemInit = (feGetEnv("TR_disableVSXMemInit") != NULL); //Disable toggle incase we break in production.
   bool use8Bytes                = ((cg->is64BitProcessor() && TR::Compiler->om.sizeofReferenceAddress() == 4) || TR::Compiler->om.sizeofReferenceAddress() == 8);
   bool useVectorStores          = (TR::Compiler->target.cpu.id() >= TR_PPCp8 && TR::Compiler->target.cpu.getPPCSupportsVSX() && !disableVSXMemInit);

   if (useVectorStores)
      {
      int32_t wordsUnrolledPerIteration = (128 * 4) / (TR::Compiler->om.sizeofReferenceAddress() * 8); // (number of bits cleared by one iteration using 4 stxvw4x) / (number of bits per word i.e. bits in a java pointer)
      if (wordsToUnroll >= wordsUnrolledPerIteration)
         {
         TR::RealRegister *vectorNullReg = cg->machine()->getPPCRealRegister(TR::RealRegister::vr0);
         cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlxor, node, vectorNullReg, vectorNullReg, vectorNullReg, cursor);

         int32_t loopIterations = wordsToUnroll / wordsUnrolledPerIteration;
         wordsToUnroll = wordsToUnroll % wordsUnrolledPerIteration;

         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, gr12, baseInitReg, initSlotOffset, cursor);
         baseInitReg = gr12;
         initSlotOffset = 0;

         if (loopIterations > 1)
            {
            cursor = loadConstant(cg, node, loopIterations, gr11, cursor);
            cursor = generateSrc1Instruction(cg, TR::InstOpCode::mtctr, node, gr11, 0, cursor);
            cursor = loadConstant(cg, node, 16, gr11, cursor); // r11 is now free so we can use it for const 16 offset
            cursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel, cursor);
            }
         else
            {
            cursor = loadConstant(cg, node, 16, gr11, cursor);
            }

         cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stxvw4x, node, new (trHeapMemory) TR::MemoryReference(nullReg, baseInitReg, 16, cg), vectorNullReg, cursor);
         cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stxvw4x, node, new (trHeapMemory) TR::MemoryReference(gr11   , baseInitReg, 16, cg), vectorNullReg, cursor);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, baseInitReg, baseInitReg, 32, cursor);
         cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stxvw4x, node, new (trHeapMemory) TR::MemoryReference(nullReg, baseInitReg, 16, cg), vectorNullReg, cursor);
         cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stxvw4x, node, new (trHeapMemory) TR::MemoryReference(gr11   , baseInitReg, 16, cg), vectorNullReg, cursor);

         if (loopIterations > 1)
            {
            cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, baseInitReg, baseInitReg, 32, cursor);
            cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bdnz, node, loopLabel, cr0, cursor);
            }
         else
            {
            initSlotOffset += 32; // if no loops needed, then no need to generate the addi when we can just increase the offset
            }
         }

      if (use8Bytes && TR::Compiler->om.sizeofReferenceAddress() == 4)
           {
           // we only need to do half the work since we'll be using std to wipe two 32bit words
           // NOTE: we need to do a stw at the end if this is odd since std only stores 8 bytes at a time
           // (and we have 4 bytes leftover after the loop)
           wordsToUnroll /= 2;
           }

      // unroll any remaining words
      TR::InstOpCode::Mnemonic instruction = (use8Bytes ? TR::InstOpCode::std : TR::InstOpCode::stw);
      int32_t storeSize         = (use8Bytes ? 8 : 4);
      while (wordsToUnroll > 0)
         {
         cursor = generateMemSrc1Instruction(cg, instruction, node, new (trHeapMemory) TR::MemoryReference(baseInitReg, initSlotOffset, storeSize, cg), nullReg, cursor);
         initSlotOffset += storeSize;
         wordsToUnroll--;
         }

      // if we had an odd number of words to unroll with a 64bit arch, we need to add one more store to clear it
      // (since we used store double word)
      if ((use8Bytes && TR::Compiler->om.sizeofReferenceAddress() == 4) && (num % 2 == 1))
         {
         cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (trHeapMemory) TR::MemoryReference(baseInitReg, initSlotOffset, 4, cg), nullReg, cursor);
         }
      }
   else
      {
      if (use8Bytes && TR::Compiler->om.sizeofReferenceAddress() == 4)
         wordsToUnroll /= 2;

      if (wordsToUnroll >=4)
         {
         if (wordsToUnroll >=8)
            {
            if(baseInitReg->getRegisterNumber()== TR::RealRegister::gr14)
               {
               cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, gr12,baseInitReg, 0 , cursor);
               baseInitReg = gr12;
               }
            cursor = loadConstant(cg, node, (int32_t)(wordsToUnroll/4), gr11, cursor);
            cursor = generateSrc1Instruction(cg, TR::InstOpCode::mtctr, node, gr11, 0, cursor);
            cursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel, cursor);
            }
         // This clears (wordsToUnroll/4) * (use8Bytes ? 32: 16) bytes.
         cursor = generateMemSrc1Instruction(cg, use8Bytes? TR::InstOpCode::std : TR::InstOpCode::stw, node, new (trHeapMemory) TR::MemoryReference(baseInitReg, initSlotOffset, TR::Compiler->om.sizeofReferenceAddress(), cg), nullReg, cursor);
         cursor = generateMemSrc1Instruction(cg, use8Bytes? TR::InstOpCode::std : TR::InstOpCode::stw, node, new (trHeapMemory) TR::MemoryReference(baseInitReg, initSlotOffset+(use8Bytes?8:4), TR::Compiler->om.sizeofReferenceAddress(), cg), nullReg, cursor);
         cursor = generateMemSrc1Instruction(cg, use8Bytes? TR::InstOpCode::std : TR::InstOpCode::stw, node, new (trHeapMemory) TR::MemoryReference(baseInitReg, initSlotOffset+(use8Bytes?16:8), TR::Compiler->om.sizeofReferenceAddress(), cg), nullReg, cursor);
         cursor = generateMemSrc1Instruction(cg, use8Bytes? TR::InstOpCode::std : TR::InstOpCode::stw, node, new (trHeapMemory) TR::MemoryReference(baseInitReg, initSlotOffset+(use8Bytes?24:12), TR::Compiler->om.sizeofReferenceAddress(), cg), nullReg, cursor);
         if (wordsToUnroll >=8)
            {
            cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, gr12, gr12, (use8Bytes?32:16), cursor);
            cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bdnz, node, loopLabel, cr0, cursor);
            }
         else
            {
            initSlotOffset += (use8Bytes?32:16);
            }
         }
      // Clears ((use8Bytes ? 8 : 4)* (wordsToUnroll % 4)) bytes
      switch (wordsToUnroll % 4)
         {
         case 3:
            cursor = generateMemSrc1Instruction(cg, use8Bytes? TR::InstOpCode::std : TR::InstOpCode::stw, node, new (trHeapMemory) TR::MemoryReference(baseInitReg, initSlotOffset+(use8Bytes?16:8), TR::Compiler->om.sizeofReferenceAddress(), cg), nullReg, cursor);
         case 2:
            cursor = generateMemSrc1Instruction(cg, use8Bytes? TR::InstOpCode::std : TR::InstOpCode::stw, node, new (trHeapMemory) TR::MemoryReference(baseInitReg, initSlotOffset+(use8Bytes?8:4), TR::Compiler->om.sizeofReferenceAddress(), cg), nullReg, cursor);
         case 1:
            cursor = generateMemSrc1Instruction(cg, use8Bytes? TR::InstOpCode::std : TR::InstOpCode::stw, node, new (trHeapMemory) TR::MemoryReference(baseInitReg, initSlotOffset, TR::Compiler->om.sizeofReferenceAddress(), cg), nullReg, cursor);
         break;
         }
      // Last one if needed
      if (use8Bytes && TR::Compiler->om.sizeofReferenceAddress() == 4 && num % 2 == 1)
         {
         if (wordsToUnroll %4)
            initSlotOffset += (use8Bytes?8:4)*(wordsToUnroll%4);
         cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (trHeapMemory) TR::MemoryReference(baseInitReg, initSlotOffset, TR::Compiler->om.sizeofReferenceAddress(), cg), nullReg, cursor);
         }
      }
   return cursor;
   }

// routines for shrink wrapping
//
static int32_t calculateFrameSize(TR::RealRegister::RegNum &intSavedFirst,
                                    int32_t &argSize,
                                    int32_t &size,
                                    int32_t &regSaveOffset,
                                    int32_t &saveSize,
                                    TR::CodeGenerator *cg,
                                    bool &saveLR)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR::Machine *machine = cg->machine();
   const TR::PPCLinkageProperties& properties = cg->getProperties();
   int32_t                    firstLocalOffset = properties.getOffsetToFirstLocal();
   int32_t                    registerSaveDescription = 0;

   // Currently no non-volatile FPR's in private linkage
   // If we change the private linkage to add non-volatile FPR's or CCR's
   // we should clean up this code to be independent of the linkage
   // (ie, to test properties.getPreserved).
   //
   saveLR = (!cg->getSnippetList().empty() ||
             comp->getJittedMethodSymbol()->isEHAware()        ||
             cg->canExceptByTrap()           ||
             machine->getLinkRegisterKilled());

   if (0 && TR::Compiler->target.is64Bit())
      {
      argSize = (cg->getLargestOutgoingArgSize() * 2) + properties.getOffsetToFirstParm();
      }
   else
      {
      argSize = cg->getLargestOutgoingArgSize() + properties.getOffsetToFirstParm();
      }

   while (intSavedFirst<=TR::RealRegister::LastGPR && !machine->getPPCRealRegister(intSavedFirst)->getHasBeenAssignedInMethod())
      intSavedFirst=(TR::RealRegister::RegNum)((uint32_t)intSavedFirst+1);

   // shrinkwrapping
   // the registerSaveDescription is emitted as follows:
   // 0000 0000 0000 000 0 0000 0000 0000 0000
   //                    <----           ---->
   //                        17 bits for saved GPRs (r15 - r32)
   //                        so the first bit represents whether r15 is saved & so on
   // <---          --->
   //     15 bits to represent the save offset from bp
   //     allowing 2^15 bytes / 8 = 4096 locals in 64-bit
   //
   if (1)
      {
      cg->setLowestSavedRegister(intSavedFirst);
      for (int32_t i = intSavedFirst; i <= TR::RealRegister::LastGPR; i++)
         {
         registerSaveDescription |= 1 << (i - TR::RealRegister::gr15);
         }
      }

   // Currently no non-volatile FPR's in private linkage

   saveSize += (TR::RealRegister::LastGPR - intSavedFirst + 1) * TR::Compiler->om.sizeofReferenceAddress();

   size += saveSize+argSize;
   if (!saveLR && size == -firstLocalOffset)   // TODO: have a emptyFrame bool to be used throughout
      {
      TR_ASSERT(argSize==0, "Wrong descriptor setting");
      size = 0;
      cg->setFrameSizeInBytes(0);
      }
   else
      {
      //Align the stack frame
      const uint32_t alignBytes = 16;
      size = (size + (alignBytes - 1) & (~(alignBytes - 1)));

      // shrinkwrapping
      regSaveOffset = (size-argSize+firstLocalOffset);

      //In MethodMetaData jitAddSpilledRegisters, we lookup the regSaveOffset
      //which must match up to what we have here or else the stackwalker will
      //run into errors.
      //Fail the compilation if we overflow the 15 bits of regSaveOffset.
      if(regSaveOffset > 0x7FFF || regSaveOffset < 0 )
         {
         //Fail if we underflow or overflow.
         comp->failCompilation<TR::CompilationInterrupted>("Overflowed or underflowed bounds of regSaveOffset in calculateFrameSize.");
         }

      //pramod
      traceMsg(comp, "PPCLinkage calculateFrameSize registerSaveDescription: 0x%x regSaveOffset: %x\n", registerSaveDescription, regSaveOffset);
      registerSaveDescription |= (regSaveOffset << 17); // see above for details
      cg->setFrameSizeInBytes(size+firstLocalOffset);
      TR_ASSERT((size-argSize+firstLocalOffset)<2048*1024, "Descriptor overflowed.\n");
      }

   return registerSaveDescription;
   }

TR::Instruction *
TR::PPCPrivateLinkage::composeSavesRestores(TR::Instruction *start,
                                           int32_t firstReg,
                                           int32_t lastReg,
                                           int32_t offset,
                                           int32_t numRegs, bool doSaves)
   {
   TR::Machine *machine = cg()->machine();
   TR::RealRegister       *stackPtr = cg()->getStackPointerRegister();
   TR::Node                 *firstNode = comp()->getStartTree()->getNode();

   TR::RealRegister::RegNum first = (TR::RealRegister::RegNum)firstReg;
   TR::RealRegister::RegNum last  = (TR::RealRegister::RegNum)lastReg;

   if (offset == -1)
      offset = getStackOffsetForReg(firstReg);
   TR::MemoryReference *pmr = new (trHeapMemory()) TR::MemoryReference(stackPtr, offset, 4*(last - first + 1), cg());
   if (!doSaves)
      start = generateTrg1MemInstruction(cg(), TR::InstOpCode::lmw, firstNode, machine->getPPCRealRegister(first), pmr, start);
   else
      start = generateMemSrc1Instruction(cg(), TR::InstOpCode::stmw, firstNode, pmr, machine->getPPCRealRegister(first), start);
   return start;
   }

bool TR::PPCPrivateLinkage::mapPreservedRegistersToStackOffsets(int32_t *mapRegsToStack, int32_t &numPreserved, TR_BitVector *&preservedRegsInLinkage)
   {
   // this routine provides a mapping between the preserved registers and
   // their location on the stack ; so shrinkWrapping can use the right
   // offsets when sinking the save/restores
   //
   TR::Machine *machine = cg()->machine();
   const TR::PPCLinkageProperties& properties = getProperties();
   bool traceIt    = comp()->getOption(TR_TraceShrinkWrapping);


   if (1)
      {
      if (traceIt)
         traceMsg(comp(), "Preserved registers for this linkage: { ");
      for (int32_t pindex = TR::RealRegister::gr15;
            pindex <= TR::RealRegister::gr31;
            pindex++)
         {
         TR::RealRegister::RegNum idx = (TR::RealRegister::RegNum)pindex;
         preservedRegsInLinkage->set(idx);
         if (traceIt)
            traceMsg(comp(), "%s ", comp()->getDebug()->getRealRegisterName(idx-1));
         }

      if (traceIt)
         traceMsg(comp(), "}\n");
      }

   TR::RealRegister::RegNum intSavedFirst = TR::RealRegister::gr15;
   int32_t frameSize = -(comp()->getJittedMethodSymbol()->getLocalMappingCursor());
   int32_t argSize = 0, regSaveOffset = 0, saveSize = 0;
   bool saveLR = false;

   //compute the frame size and stash it away in the linkage
   //
   int32_t registerSaveDescription = calculateFrameSize(intSavedFirst, argSize, frameSize, regSaveOffset, saveSize, cg(), saveLR);
   setRegisterSaveSize(regSaveOffset);

   //pramod
   traceMsg(comp(), "regSaveOffset %d\n", regSaveOffset);


   for (int32_t i = intSavedFirst; i <= TR::RealRegister::LastGPR; i++)
      {
      mapRegsToStack[i] = argSize;
      setStackOffsetForReg(i, argSize);
      traceMsg(comp(), "idx %d is assigned gets offsetcursor %d\n", i, argSize);
      argSize = argSize + TR::Compiler->om.sizeofReferenceAddress();
      }

   // return true or false depending on whether
   // the linkage uses pushes for preserved regs
   return false;
   }

TR::Instruction *
TR::PPCPrivateLinkage::savePreservedRegister(TR::Instruction *cursor, int32_t regIndex, int32_t offset)
   {
   TR::Node *n = comp()->getStartTree()->getNode();
   TR::RealRegister       *stackPtr = cg()->getStackPointerRegister();
   TR::Machine *machine = cg()->machine();

   if (offset == -1)
      offset = getStackOffsetForReg(regIndex);
   cursor = generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_st, n, new (trHeapMemory()) TR::MemoryReference(stackPtr, offset, TR::Compiler->om.sizeofReferenceAddress(), cg()), machine->getPPCRealRegister((TR::RealRegister::RegNum)regIndex), cursor);
   return cursor;
   }

TR::Instruction *
TR::PPCPrivateLinkage::restorePreservedRegister(TR::Instruction *cursor, int32_t regIndex, int32_t offset)
   {
   TR::Node *n = comp()->getStartTree()->getNode();
   TR::RealRegister       *stackPtr = cg()->getStackPointerRegister();
   TR::Machine *machine = cg()->machine();

   if (offset == -1)
      offset = getStackOffsetForReg(regIndex);
   cursor = generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, n, machine->getPPCRealRegister((TR::RealRegister::RegNum)regIndex), new (trHeapMemory()) TR::MemoryReference(stackPtr, offset, TR::Compiler->om.sizeofReferenceAddress(), cg()), cursor);
   return cursor;
   }

void TR::PPCPrivateLinkage::createPrologue(TR::Instruction *cursor)
   {
   TR::Machine *machine = cg()->machine();
   const TR::PPCLinkageProperties& properties = getProperties();
   TR::ResolvedMethodSymbol      *bodySymbol = comp()->getJittedMethodSymbol();
   TR::RealRegister        *stackPtr = cg()->getStackPointerRegister();
   TR::RealRegister        *metaBase = cg()->getMethodMetaDataRegister();
   TR::RealRegister        *gr0 = machine->getPPCRealRegister(TR::RealRegister::gr0);
   TR::RealRegister        *gr11 = machine->getPPCRealRegister(TR::RealRegister::gr11);
   TR::RealRegister        *gr12 = machine->getPPCRealRegister(TR::RealRegister::gr12);
   TR::RealRegister        *cr0 = machine->getPPCRealRegister(TR::RealRegister::cr0);
   TR::Node                   *firstNode = comp()->getStartTree()->getNode();
   int32_t                    size = -(bodySymbol->getLocalMappingCursor());
   int32_t                    residualSize, saveSize=0, argSize;
   int32_t                    registerSaveDescription=0;
   int32_t                    firstLocalOffset = properties.getOffsetToFirstLocal();
   int                        i;
   TR::RealRegister::RegNum intSavedFirst=TR::RealRegister::gr15;
   TR::RealRegister::RegNum regIndex;
   int32_t regSaveOffset = 0;
   bool saveLR = false;
   registerSaveDescription = calculateFrameSize(intSavedFirst, argSize, size, regSaveOffset, saveSize, cg(), saveLR);

   cg()->setRegisterSaveDescription(registerSaveDescription);
   residualSize = size;

   if (comp()->getOption(TR_EntryBreakPoints))
      {
      cursor = generateInstruction(cg(), TR::InstOpCode::bad, firstNode, cursor);
      }

   bool fsd = comp()->getOption(TR_FullSpeedDebug);

   // If in Full Speed Debug, the parameters have to be saved before the call to Stack Check
   // and the stack map and register maps have to be updated accordingly
   if (fsd)
      {
      // Only call saveArguments for pushing the parameters on the stack when we are on Full Speed Debug
      cursor = saveArguments(cursor, fsd, true);
      }

   // Load the stack limit offset for comparison
   if (!comp()->isDLT())
      cursor = generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, firstNode, gr11,
                  new (trHeapMemory()) TR::MemoryReference(metaBase, cg()->getStackLimitOffset(),
                  TR::Compiler->om.sizeofReferenceAddress(),
                  cg()), cursor);

   if (cg()->getFrameSizeInBytes() || saveLR)
      {
      if ((cg()->getFrameSizeInBytes() + TR::Compiler->om.sizeofReferenceAddress()) > (-LOWER_IMMED))
         {
         // Large Frame Support

         // gr12 <- (totalFrameSize + 1 slot)
         cursor = loadConstant(cg(), firstNode, (int32_t)(cg()->getFrameSizeInBytes() + TR::Compiler->om.sizeofReferenceAddress()), gr12, cursor);

         // javaSP <- javaSP - (totalFrameSize + 1 slot)
         cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::subf, firstNode, stackPtr, gr12, stackPtr, cursor);

         // gr12 == totalFrameSize
         if (!comp()->isDLT())
            cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::addi2, firstNode, gr12, gr12, -TR::Compiler->om.sizeofReferenceAddress(), cursor);

         if (saveLR)
            {
            cursor = generateTrg1Instruction(cg(), TR::InstOpCode::mflr, firstNode, gr0, cursor);
            }

         // Check for stack overflow (set a flag)
         if (!comp()->isDLT())
            cursor = generateTrg1Src2Instruction(cg(),TR::InstOpCode::Op_cmpl, firstNode, cr0, stackPtr, gr11, cursor);

         if (saveLR)
            {
            // javaSP[totalFrameSize] <- linkRegister
            cursor = generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_stx, firstNode, new (trHeapMemory()) TR::MemoryReference(stackPtr, gr12, TR::Compiler->om.sizeofReferenceAddress(), cg()),
                        gr0, cursor);
            }
         }
      else
         {
         // Small Frame Support

         // javaSP <- javaSP - (totalFrameSize + 1 slot)
         cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::addi2,
                     firstNode, stackPtr, stackPtr, -(cg()->getFrameSizeInBytes() + TR::Compiler->om.sizeofReferenceAddress()), cursor);
         if (saveLR)
            {
            cursor = generateTrg1Instruction(cg(), TR::InstOpCode::mflr, firstNode, gr0, cursor);
            }

         // Check for stack overflow (set a flag)
         if (!comp()->isDLT())
            cursor = generateTrg1Src2Instruction(cg(),TR::InstOpCode::Op_cmpl, firstNode, cr0, stackPtr, gr11, cursor);

         if (saveLR)
            {
            // javaSP[totalFrameSize] <- linkRegister
            cursor = generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_st, firstNode, new (trHeapMemory()) TR::MemoryReference(stackPtr, cg()->getFrameSizeInBytes(), TR::Compiler->om.sizeofReferenceAddress(), cg()),
                        gr0, cursor);
            }
         }
      }
   else
      {
      // Empty Frame Support

      // Check for stack overflow (set a flag)
      if (!comp()->isDLT())
         cursor = generateTrg1Src2Instruction(cg(),TR::InstOpCode::Op_cmpl, firstNode, cr0, stackPtr, gr11, cursor);
      }

   if (!comp()->isDLT())
      {
      TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg());
      TR::LabelSymbol *reStartLabel = generateLabelSymbol(cg());

      // Branch to StackOverflow snippet if javaSP > stack limit
      if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
         // use PPC AS branch hint
         cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::ble, PPCOpProp_BranchUnlikely, firstNode, snippetLabel, cr0, cursor);
      else
         cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::ble, firstNode, snippetLabel, cr0, cursor);
      TR::Snippet *snippet = new (trHeapMemory()) TR::PPCStackCheckFailureSnippet(cg(), firstNode, reStartLabel, snippetLabel);
      cg()->addSnippet(snippet);
      cursor = generateLabelInstruction(cg(), TR::InstOpCode::label, firstNode, reStartLabel, cursor);
      }

   if (intSavedFirst <= TR::RealRegister::LastGPR)
      {
      if (TR::Compiler->target.is64Bit() ||
           !comp()->getOption(TR_DisableShrinkWrapping) ||
          (!comp()->getOption(TR_OptimizeForSpace) &&
           TR::RealRegister::LastGPR - intSavedFirst <= 3))
         {
         // shrink wrapping
         TR_BitVector *p = cg()->getPreservedRegsInPrologue();
         for (regIndex=intSavedFirst; regIndex<=TR::RealRegister::LastGPR; regIndex=(TR::RealRegister::RegNum)((uint32_t)regIndex+1))
            {
            if (!p || p->get((uint32_t)regIndex))
               cursor = generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_st, firstNode, new (trHeapMemory()) TR::MemoryReference(stackPtr, argSize, TR::Compiler->om.sizeofReferenceAddress(), cg()), machine->getPPCRealRegister(regIndex), cursor);
            argSize = argSize + TR::Compiler->om.sizeofReferenceAddress();
            }
         }
      else
         {
         if (comp()->getOption(TR_DisableShrinkWrapping) ||
               !cg()->getPreservedRegsInPrologue())
            cursor = generateMemSrc1Instruction(cg(), (intSavedFirst==TR::RealRegister::LastGPR)?TR::InstOpCode::stw:TR::InstOpCode::stmw, firstNode, new (trHeapMemory()) TR::MemoryReference(stackPtr, argSize, 4*(TR::RealRegister::LastGPR-intSavedFirst+1), cg()), machine->getPPCRealRegister(intSavedFirst), cursor);
         else
            cursor = cg()->saveOrRestoreRegisters(cg()->getPreservedRegsInPrologue(), cursor, true);
         argSize += (TR::RealRegister::LastGPR - intSavedFirst + 1) * 4;
         }
      }


   TR::GCStackAtlas *atlas = cg()->getStackAtlas();
   if (atlas != NULL)
      {
      //
      // Note that since internal pointer support has been added, all pinning
      // array autos and derived internal pointer autos must be zero initialized
      // in prologue regardless of the liveness information. This is because
      // the stack walker/GC examine internal pointer related slots whenever GC
      // occurs. Having the slots in uninitialized state is not correct for GC.
      //
      uint32_t numLocalsToBeInitialized = atlas->getNumberOfSlotsToBeInitialized();
      if ((numLocalsToBeInitialized > 0) ||
          atlas->getInternalPointerMap())
         {
         bool adjustedFrameOnce = false;
         TR::RealRegister *nullValueRegister = gr0;
         int32_t            offset = atlas->getLocalBaseOffset();

         // Note: gr0 will not be suitable if NULLVALUE is out of [LOWER, UPPER)
         cursor = loadConstant(cg(), firstNode, NULLVALUE, nullValueRegister, cursor);

         static bool disableUnrollPrologueInitLoop = (feGetEnv("TR_DisableUnrollPrologueInitLoop") != NULL);
         if (numLocalsToBeInitialized > 5)
            {
            int32_t loopDistance = -numLocalsToBeInitialized * TR::Compiler->om.sizeofReferenceAddress();
            int32_t initBottomOffset = offset - loopDistance;
            int32_t initSlotOffset =0;

            TR_ASSERT(initBottomOffset<=0, "Initialized and internal pointer portion size doesn't make senses");
            if (residualSize >= -LOWER_IMMED)
               {
               cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::add, firstNode, gr12, gr12, stackPtr, cursor);
               adjustedFrameOnce = true;
               if (size == residualSize)     // only happens when LR is not saved
                  {
                  residualSize -= TR::Compiler->om.sizeofReferenceAddress();
                  initBottomOffset -= TR::Compiler->om.sizeofReferenceAddress();
                  }
               if (initBottomOffset < LOWER_IMMED)
                  cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::addis, firstNode, gr12, gr12, HI_VALUE(initBottomOffset), cursor);
               if (initBottomOffset != 0)
                  cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::addi2, firstNode, gr12, gr12, LO_VALUE(initBottomOffset), cursor);
               }
            else
               {
               adjustedFrameOnce = true;
               if (size == residualSize)     // only happens when LR is not saved
                  residualSize -= TR::Compiler->om.sizeofReferenceAddress();
               initSlotOffset = initSlotOffset+ residualSize+initBottomOffset;
               }

            TR::LabelSymbol *loopLabel = generateLabelSymbol(cg());
            if (!disableUnrollPrologueInitLoop)
               {
               initSlotOffset += loopDistance;
               if (residualSize >= -LOWER_IMMED)
                  cursor = unrollPrologueInitLoop(cg(), firstNode, numLocalsToBeInitialized, loopDistance, nullValueRegister, gr11, gr12, gr12, cr0, loopLabel, cursor);
               else
                  cursor = unrollPrologueInitLoop(cg(), firstNode, numLocalsToBeInitialized, initSlotOffset, nullValueRegister, gr11, stackPtr, gr12, cr0, loopLabel, cursor);
               }
            else
               {
               cursor = loadConstant(cg(), firstNode, loopDistance, gr11, cursor);
               cursor = generateLabelInstruction(cg(), TR::InstOpCode::label, firstNode, loopLabel, cursor);
               if (residualSize >= -LOWER_IMMED)
                  cursor = generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_stx, firstNode, new (trHeapMemory()) TR::MemoryReference(gr12, gr11, TR::Compiler->om.sizeofReferenceAddress(), cg()), nullValueRegister, cursor);
               else
                  cursor = generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_stx, firstNode, new (trHeapMemory()) TR::MemoryReference(stackPtr, gr11, initSlotOffset+ TR::Compiler->om.sizeofReferenceAddress(), cg()), nullValueRegister, cursor);
               cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::addic_r, firstNode, gr11, gr11, cr0, TR::Compiler->om.sizeofReferenceAddress(), cursor);
               cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::bne, firstNode, loopLabel, cr0, cursor);
               }
            }
         else
            {
            adjustedFrameOnce = true;
            if (size == residualSize)        // only happens when LR is not saved
               residualSize -= TR::Compiler->om.sizeofReferenceAddress();
            bool use8BytesOn32Bit = cg()->is64BitProcessor() && (TR::Compiler->om.sizeofReferenceAddress() == 4);
            if (!disableUnrollPrologueInitLoop && use8BytesOn32Bit)
               {
               int32_t count = 0;
               if (numLocalsToBeInitialized >= 2)
                  {
                  cursor = generateMemSrc1Instruction(cg(), TR::InstOpCode::std, firstNode, new (trHeapMemory()) TR::MemoryReference(stackPtr, offset+residualSize, TR::Compiler->om.sizeofReferenceAddress(), cg()), nullValueRegister, cursor);
                  count++;
                  if (numLocalsToBeInitialized >= 4)
                     {
                     cursor = generateMemSrc1Instruction(cg(), TR::InstOpCode::std, firstNode, new (trHeapMemory()) TR::MemoryReference(stackPtr, offset+residualSize+8, TR::Compiler->om.sizeofReferenceAddress(), cg()), nullValueRegister, cursor);
                     count++;
                     }
                  }
               if (numLocalsToBeInitialized % 2 == 1)
                  cursor = generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_st, firstNode, new (trHeapMemory()) TR::MemoryReference(stackPtr, offset+residualSize+(count*8), TR::Compiler->om.sizeofReferenceAddress(), cg()), nullValueRegister, cursor);
               }
            else
               {
               for (i = 0; i < numLocalsToBeInitialized; i++, offset += TR::Compiler->om.sizeofReferenceAddress())
                  {
                  cursor = generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_st, firstNode, new (trHeapMemory()) TR::MemoryReference(stackPtr, offset+residualSize, TR::Compiler->om.sizeofReferenceAddress(), cg()), nullValueRegister, cursor);
                  }
               }
            }

         if (atlas->getInternalPointerMap())
            {
            int32_t offset = atlas->getOffsetOfFirstInternalPointer();

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

            if (!adjustedFrameOnce)
               {
               if (size == residualSize)        // only happens when LR is not saved
                  residualSize -= TR::Compiler->om.sizeofReferenceAddress();
               }
            if (!disableUnrollPrologueInitLoop)
               {
               TR::LabelSymbol *loopLabel = generateLabelSymbol(cg());
               cursor = unrollPrologueInitLoop(cg(), firstNode, numSlotsToBeInitialized,  offset+residualSize, nullValueRegister, gr11, stackPtr, gr12, cr0, loopLabel, cursor);
               }
            else
               {
               for (i = 0; i < numSlotsToBeInitialized; i++, offset += TR::Compiler->om.sizeofReferenceAddress())
                  {
                  cursor = generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_st, firstNode, new (trHeapMemory()) TR::MemoryReference(stackPtr, offset+residualSize, TR::Compiler->om.sizeofReferenceAddress(), cg()), nullValueRegister, cursor);
                  }
               }
            }
         }
      }

   TR_ASSERT(size<=UPPER_IMMED-1032, "Setting up a frame pointer anyway.");
   ListIterator<TR::AutomaticSymbol>  automaticIterator(&bodySymbol->getAutomaticList());
   TR::AutomaticSymbol               *localCursor       = automaticIterator.getFirst();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());

   while (localCursor!=NULL)
      {
      TR_ASSERT(!comp()->useCompressedPointers() ||
      !localCursor->isLocalObject() ||
      !localCursor->isCollectedReference() ||
      (localCursor->getOffset() & (fej9->getObjectAlignmentInBytes() - 1)) == 0,
      "Stack allocated object not aligned to minimum required alignment");
      localCursor->setOffset(localCursor->getOffset() + size);
      localCursor = automaticIterator.getNext();
      }

   ListIterator<TR::ParameterSymbol> parameterIterator(&bodySymbol->getParameterList());
   TR::ParameterSymbol              *parmCursor = parameterIterator.getFirst();
   while (parmCursor != NULL)
      {
      parmCursor->setParameterOffset(parmCursor->getParameterOffset() + size);
      parmCursor = parameterIterator.getNext();
      }

   // Save or move arguments according to the result of register assignment.
   // If in Full Speed Debug mode, it will not push the arguments to the stack (which was done
   // above) but it will complete the other operations
   // If not in Full Speed Debug mode, it will execute all operations in saveArguments
   cursor = saveArguments(cursor, fsd, false);

   if (atlas != NULL)
      {
      TR_GCStackMap *map = atlas->getLocalMap();
      map->setLowestOffsetInstruction(cursor);
      if (!comp()->useRegisterMaps())
         atlas->addStackMap(map);
      }
   }

TR::MemoryReference *TR::PPCPrivateLinkage::getOutgoingArgumentMemRef(int32_t argSize, TR::Register *argReg, TR::InstOpCode::Mnemonic opCode, TR::PPCMemoryArgument &memArg, uint32_t length)
   {
   TR::MemoryReference *result=new (trHeapMemory()) TR::MemoryReference(cg()->getStackPointerRegister(), argSize, length, cg());
   memArg.argRegister = argReg;
   memArg.argMemory = result;
   memArg.opCode = opCode;
   return(result);
   }

void TR::PPCPrivateLinkage::createEpilogue(TR::Instruction *cursor)
   {
   int32_t                   blockNumber = cursor->getNext()->getBlockIndex();
   TR::Machine *machine = cg()->machine();
   const TR::PPCLinkageProperties& properties = getProperties();
   TR::ResolvedMethodSymbol      *bodySymbol = comp()->getJittedMethodSymbol();
   TR::RealRegister        *stackPtr = cg()->getStackPointerRegister();
   TR::RealRegister        *gr12 = machine->getPPCRealRegister(TR::RealRegister::gr12);
   TR::RealRegister        *gr0 = machine->getPPCRealRegister(TR::RealRegister::gr0);
   TR::Node                   *currentNode = cursor->getNode();
   int32_t                   saveSize;
   int32_t                   frameSize = cg()->getFrameSizeInBytes();
   TR::RealRegister::RegNum savedFirst=TR::RealRegister::gr15;
   TR::RealRegister::RegNum regIndex;
   bool                    restoreLR = (cg()->getSnippetList().size()>1 ||
                                       (comp()->isDLT() && !cg()->getSnippetList().empty()) ||
                                       bodySymbol->isEHAware()            ||
                                       cg()->canExceptByTrap()            ||
                                       machine->getLinkRegisterKilled());

   bool                     saveLR = restoreLR || machine->getLinkRegisterKilled();


   if (restoreLR && frameSize <= UPPER_IMMED)
      {
      cursor = generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, currentNode, gr0, new (trHeapMemory()) TR::MemoryReference(stackPtr, frameSize, TR::Compiler->om.sizeofReferenceAddress(), cg()), cursor);
      cursor = generateSrc1Instruction(cg(), TR::InstOpCode::mtlr, currentNode, gr0, 0, cursor);
      }

   if (0 && TR::Compiler->target.is64Bit())
      {
      saveSize = (cg()->getLargestOutgoingArgSize() * 2) + properties.getOffsetToFirstParm();
      }
   else
      {
      saveSize = cg()->getLargestOutgoingArgSize() + properties.getOffsetToFirstParm();
      }

   while (savedFirst<=TR::RealRegister::LastGPR && !machine->getPPCRealRegister(savedFirst)->getHasBeenAssignedInMethod())
      savedFirst=(TR::RealRegister::RegNum)((uint32_t)savedFirst+1);

   if (savedFirst <= TR::RealRegister::LastGPR)
      {
      if (TR::Compiler->target.is64Bit() ||
           !comp()->getOption(TR_DisableShrinkWrapping) ||
          (!comp()->getOption(TR_OptimizeForSpace) &&
           TR::RealRegister::LastGPR - savedFirst <= 3))
         {
         // shrink wrapping
         TR_BitVector *p = cg()->getPreservedRegsInPrologue();
         for (regIndex=savedFirst; regIndex<=TR::RealRegister::LastGPR; regIndex=(TR::RealRegister::RegNum)((uint32_t)regIndex+1))
            {
            if (!p || p->get((uint32_t)regIndex))
                cursor = generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, currentNode, machine->getPPCRealRegister(regIndex), new (trHeapMemory()) TR::MemoryReference(stackPtr, saveSize, TR::Compiler->om.sizeofReferenceAddress(), cg()), cursor);

            saveSize = saveSize + TR::Compiler->om.sizeofReferenceAddress();
            }
         }
      else
         {
         if (comp()->getOption(TR_DisableShrinkWrapping) ||
               !cg()->getPreservedRegsInPrologue())
            cursor = generateTrg1MemInstruction(cg(), (savedFirst==TR::RealRegister::LastGPR)?TR::InstOpCode::lwz:TR::InstOpCode::lmw, currentNode, machine->getPPCRealRegister(savedFirst), new (trHeapMemory()) TR::MemoryReference(stackPtr, saveSize, 4*(TR::RealRegister::LastGPR-savedFirst+1), cg()), cursor);
         else
            cursor = cg()->saveOrRestoreRegisters(cg()->getPreservedRegsInPrologue(), cursor, false);
         saveSize += (TR::RealRegister::LastGPR - savedFirst + 1) * 4;
         }
      }

   if (frameSize || saveLR)
      {
      saveSize = cg()->getFrameSizeInBytes() + TR::Compiler->om.sizeofReferenceAddress();
      if (saveSize > UPPER_IMMED)
         {
         cursor = loadConstant(cg(), currentNode, saveSize, gr12, cursor);
         cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::add, currentNode, stackPtr, stackPtr, gr12, cursor);
         }
      else
         {
         cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::addi2, currentNode, stackPtr, stackPtr, saveSize, cursor);
         }

      if (restoreLR && frameSize > UPPER_IMMED)
         {
         cursor = generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, currentNode, gr0, new (trHeapMemory()) TR::MemoryReference(stackPtr, -TR::Compiler->om.sizeofReferenceAddress(), TR::Compiler->om.sizeofReferenceAddress(), cg()), cursor);
         cursor = generateSrc1Instruction(cg(), TR::InstOpCode::mtlr, currentNode, gr0, 0, cursor);
         }
      }
   }

int32_t TR::PPCPrivateLinkage::buildArgs(TR::Node *callNode,
                                        TR::RegisterDependencyConditions *dependencies)
   {
   return buildPrivateLinkageArgs(callNode, dependencies, TR_Private);
   }

int32_t TR::PPCPrivateLinkage::buildPrivateLinkageArgs(TR::Node                         *callNode,
                                                       TR::RegisterDependencyConditions *dependencies,
                                                       TR_LinkageConventions             linkage)
   {
   TR_ASSERT(linkage == TR_Private || linkage == TR_Helper || linkage == TR_CHelper, "Unexpected linkage convention");

   const TR::PPCLinkageProperties&  properties = getProperties();
   TR::PPCMemoryArgument           *pushToMemory = NULL;
   TR::Register                    *tempRegister;
   int32_t                          argIndex = 0, memArgs = 0, from, to, step;
   int32_t                          argSize = -properties.getOffsetToFirstParm(), totalSize = 0;
   uint32_t                         numIntegerArgs = 0;
   uint32_t                         numFloatArgs = 0;
   uint32_t                         firstExplicitArg = 0;
   TR::Node                        *child;
   void                            *smark;
   uint32_t                         firstArgumentChild = callNode->getFirstArgumentIndex();
   TR::DataType                     resType = callNode->getType();
   TR_Array<TR::Register *>&        tempLongRegisters = cg()->getTransientLongRegisters();
   TR::MethodSymbol                *callSymbol = callNode->getSymbol()->castToMethodSymbol();

   bool isHelperCall = linkage == TR_Helper || linkage == TR_CHelper;
   bool rightToLeft = isHelperCall &&
                      //we want the arguments for induceOSR to be passed from left to right as in any other non-helper call
                      callNode->getSymbolReference() != cg()->symRefTab()->element(TR_induceOSRAtCurrentPC);

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

   if (!properties.getPreserved(TR::RealRegister::gr2))
      {
      // Helper linkage preserves all registers that are not argument registers, so we don't need to spill them.
      if (linkage != TR_Helper)
         addDependency(dependencies, NULL, TR::RealRegister::gr2, TR_GPR, cg());
      }

   uint32_t numIntArgRegs   = properties.getNumIntArgRegs();
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
      if (comp()->getOption(TR_TraceCG))
         {
         traceMsg(comp(), "Special arg %s in %s\n",
            comp()->getDebug()->getName(callNode->getChild(from)),
            comp()->getDebug()->getName(cg()->machine()->getPPCRealRegister(specialArgReg)));
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
      switch (child->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address:
            if (numIntegerArgs >= numIntArgRegs)
               memArgs++;
            numIntegerArgs++;
            totalSize += TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Int64:
            if (TR::Compiler->target.is64Bit())
               {
               if (numIntegerArgs >= numIntArgRegs)
                  memArgs++;
               numIntegerArgs++;
               }
            else
               {
               if (numIntegerArgs+1 == numIntArgRegs)
                  memArgs++;
               else if (numIntegerArgs+1 > numIntArgRegs)
                  memArgs += 2;
               numIntegerArgs += 2;
               }
            totalSize += 2*TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Float:
            if (numFloatArgs >= numFloatArgRegs)
               memArgs++;
            numFloatArgs++;
            totalSize += TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Double:
            if (numFloatArgs >= numFloatArgRegs)
               memArgs++;
            numFloatArgs++;
            totalSize += 2*TR::Compiler->om.sizeofReferenceAddress();
            break;
         }
      }

   // From here, down, any new stack allocations will expire / die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   if (memArgs > 0)
      {
      pushToMemory = new (trStackMemory()) TR::PPCMemoryArgument[memArgs];
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
      generateTrg1Src1Instruction(cg(), TR::InstOpCode::mr, callNode, vmThreadArgRegister, cg()->getMethodMetaDataRegister());
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
      TR::MemoryReference *mref = NULL;
      TR::Register        *argRegister;
      child = callNode->getChild(i);
      bool isSpecialArg = (i == from && specialArgReg != TR::RealRegister::NoReg);
      switch (child->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address: // have to do something for GC maps here
            if (i == firstArgumentChild && callNode->getOpCode().isIndirect())
               {
               argRegister = pushThis(child);
               }
            else
               {
               if (child->getDataType() == TR::Address)
                  {
                  argRegister = pushAddressArg(child);
                  }
               else
                  {
                  argRegister = pushIntegerWordArg(child);
                  }
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
                  addDependency(dependencies, argRegister, specialArgReg, TR_GPR, cg());
                  }
               }
            else
               {
               argSize += TR::Compiler->om.sizeofReferenceAddress();
               if (numIntegerArgs < numIntArgRegs)
                  {
                  if (!cg()->canClobberNodesRegister(child, 0))
                     {
                     if (argRegister->containsCollectedReference())
                        tempRegister = cg()->allocateCollectedReferenceRegister();
                     else
                        tempRegister = cg()->allocateRegister();
                     generateTrg1Src1Instruction(cg(), TR::InstOpCode::mr, callNode, tempRegister, argRegister);
                     argRegister = tempRegister;
                     }
                  if (numIntegerArgs == firstExplicitArg &&
                      (resType.isInt32() || resType.isInt64() || resType.isAddress()))
                     {
                     TR::Register *resultReg;
                     if (resType.isAddress())
                        resultReg = cg()->allocateCollectedReferenceRegister();
                     else
                        resultReg = cg()->allocateRegister();
                     dependencies->addPreCondition(argRegister, properties.getIntegerArgumentRegister(numIntegerArgs));
                     dependencies->addPostCondition(resultReg, TR::RealRegister::gr3);
                     if (firstExplicitArg == 1)
                        dependencies->addPostCondition(argRegister, properties.getIntegerArgumentRegister(numIntegerArgs));
                     }
                  else if (TR::Compiler->target.is32Bit() && numIntegerArgs == (firstExplicitArg + 1) && resType.isInt64())
                     {
                     TR::Register *resultReg = cg()->allocateRegister();
                     dependencies->addPreCondition(argRegister, properties.getIntegerArgumentRegister(numIntegerArgs));
                     dependencies->addPostCondition(resultReg, TR::RealRegister::gr4);
                     if (firstExplicitArg == 1)
                        dependencies->addPostCondition(argRegister, properties.getIntegerArgumentRegister(numIntegerArgs));
                     }
                  else
                     {
                     addDependency(dependencies, argRegister, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
                     }
                  }
               else // numIntegerArgs >= numIntArgRegs
                  {
                  if (child->getDataType() == TR::Address)
                     mref = getOutgoingArgumentMemRef(totalSize-argSize, argRegister,TR::InstOpCode::Op_st, pushToMemory[argIndex++], TR::Compiler->om.sizeofReferenceAddress());
                  else
                     mref = getOutgoingArgumentMemRef(totalSize-argSize, argRegister, TR::InstOpCode::stw, pushToMemory[argIndex++], 4);
                  }
               numIntegerArgs++;
               }
            break;
         case TR::Int64:
            argRegister = pushLongArg(child);
            if (isSpecialArg)
               {
               // Note: special arg regs use only one reg even on 32-bit platforms.
               // If the special arg is of type TR::Int64, that only means we don't
               // care about the top 32 bits.
               TR::Register *specialArgRegister = argRegister->getRegisterPair()? argRegister->getLowOrder() : argRegister;
               if (specialArgReg == properties.getIntegerReturnRegister(0))
                  {
                  TR::Register *resultReg;
                  if (resType.isAddress())
                     resultReg = cg()->allocateCollectedReferenceRegister();
                  else
                     resultReg = cg()->allocateRegister();
                  dependencies->addPreCondition(specialArgRegister, specialArgReg);
                  dependencies->addPostCondition(resultReg, properties.getIntegerReturnRegister(0));
                  }
               else
                  {
                  addDependency(dependencies, specialArgRegister, specialArgReg, TR_GPR, cg());
                  }
               }
            else
               {
               argSize += 2*TR::Compiler->om.sizeofReferenceAddress();
               if (numIntegerArgs < numIntArgRegs)
                  {
                  if (!cg()->canClobberNodesRegister(child, 0))
                     {
                     if (TR::Compiler->target.is64Bit())
                        {
                        if (argRegister->containsCollectedReference())
                           tempRegister = cg()->allocateCollectedReferenceRegister();
                        else
                           tempRegister = cg()->allocateRegister();
                        generateTrg1Src1Instruction(cg(), TR::InstOpCode::mr, callNode, tempRegister, argRegister);
                        argRegister = tempRegister;
                        }
                     else
                        {
                        tempRegister = cg()->allocateRegister();
                        generateTrg1Src1Instruction(cg(), TR::InstOpCode::mr, callNode, tempRegister, argRegister->getRegisterPair()->getHighOrder());
                        argRegister = cg()->allocateRegisterPair(argRegister->getRegisterPair()->getLowOrder(), tempRegister);
                        tempLongRegisters.add(argRegister);
                        }
                     }
                  if (numIntegerArgs == firstExplicitArg && (resType.isInt32() || resType.isInt64() || resType.isAddress()))
                     {
                     TR::Register *resultReg;
                     if (resType.isAddress())
                        resultReg = cg()->allocateCollectedReferenceRegister();
                     else
                        resultReg = cg()->allocateRegister();
                     if (TR::Compiler->target.is64Bit())
                        dependencies->addPreCondition(argRegister, properties.getIntegerArgumentRegister(numIntegerArgs));
                     else
                        dependencies->addPreCondition(argRegister->getRegisterPair()->getHighOrder(), properties.getIntegerArgumentRegister(numIntegerArgs));
                     dependencies->addPostCondition(resultReg, TR::RealRegister::gr3);
                     if (firstExplicitArg == 1)
                        dependencies->addPostCondition(argRegister, properties.getIntegerArgumentRegister(numIntegerArgs));
                     }
                  else if (TR::Compiler->target.is32Bit() && numIntegerArgs == (firstExplicitArg + 1) && resType.isInt64())
                     {
                     TR::Register *resultReg = cg()->allocateRegister();
                     dependencies->addPreCondition(argRegister->getRegisterPair()->getHighOrder(), properties.getIntegerArgumentRegister(numIntegerArgs));
                     dependencies->addPostCondition(resultReg, TR::RealRegister::gr4);
                     if (firstExplicitArg == 1)
                        dependencies->addPostCondition(argRegister, properties.getIntegerArgumentRegister(numIntegerArgs));
                      }
                  else
                     {
                     if (TR::Compiler->target.is64Bit())
                        addDependency(dependencies, argRegister, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
                     else
                        addDependency(dependencies, argRegister->getRegisterPair()->getHighOrder(), properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
                     }
                  if (TR::Compiler->target.is32Bit())
                     {
                     if (numIntegerArgs+1 < numIntArgRegs)
                        {
                        if (!cg()->canClobberNodesRegister(child, 0))
                           {
                           TR::Register *over_lowReg = argRegister->getRegisterPair()->getLowOrder();
                           tempRegister = cg()->allocateRegister();
                           generateTrg1Src1Instruction(cg(), TR::InstOpCode::mr, callNode, tempRegister, over_lowReg);
                           argRegister->getRegisterPair()->setLowOrder(tempRegister, cg());
                           }
                        if (numIntegerArgs == firstExplicitArg && resType.isInt64())
                           {
                           TR::Register *resultReg = cg()->allocateRegister();
                           dependencies->addPreCondition(argRegister->getRegisterPair()->getLowOrder(), properties.getIntegerArgumentRegister(numIntegerArgs + 1));
                           dependencies->addPostCondition(resultReg, TR::RealRegister::gr4);
                           if (firstExplicitArg == 1)
                              dependencies->addPostCondition(argRegister, properties.getIntegerArgumentRegister(numIntegerArgs + 1));
                           }
                        else
                           addDependency(dependencies, argRegister->getRegisterPair()->getLowOrder(), properties.getIntegerArgumentRegister(numIntegerArgs+1), TR_GPR, cg());
                        }
                     else // numIntegerArgs+1 == numIntArgRegs
                        mref = getOutgoingArgumentMemRef(totalSize-argSize+4, argRegister->getRegisterPair()->getLowOrder(), TR::InstOpCode::stw, pushToMemory[argIndex++], 4);
                     numIntegerArgs++;
                     }
                  }
               else // numIntegerArgs >= numIntArgRegs
                  {
                  if (TR::Compiler->target.is64Bit())
                     mref = getOutgoingArgumentMemRef(totalSize-argSize, argRegister, TR::InstOpCode::std, pushToMemory[argIndex++], 8);
                  else
                     {
                     mref = getOutgoingArgumentMemRef(totalSize-argSize, argRegister->getRegisterPair()->getHighOrder(), TR::InstOpCode::stw, pushToMemory[argIndex++], 4);
                     mref = getOutgoingArgumentMemRef(totalSize-argSize+4, argRegister->getRegisterPair()->getLowOrder(), TR::InstOpCode::stw, pushToMemory[argIndex++], 4);
                     numIntegerArgs++;
                     }
                  }
               numIntegerArgs++;
               }
            break;
         case TR::Float:
            argSize += TR::Compiler->om.sizeofReferenceAddress();
            argRegister = pushFloatArg(child);
            if (numFloatArgs < numFloatArgRegs)
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  tempRegister = cg()->allocateRegister(TR_FPR);
                  generateTrg1Src1Instruction(cg(), TR::InstOpCode::fmr, callNode, tempRegister, argRegister);
                  argRegister = tempRegister;
                  }
               if (numFloatArgs == 0 && resType.isFloatingPoint())
                  {
                  TR::Register *resultReg;
                  if (resType.getDataType() == TR::Float)
                     resultReg = cg()->allocateSinglePrecisionRegister();
                  else
                     resultReg = cg()->allocateRegister(TR_FPR);
                  dependencies->addPreCondition(argRegister, TR::RealRegister::fp0);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::fp0);
                  }
               else
                  addDependency(dependencies, argRegister, properties.getFloatArgumentRegister(numFloatArgs), TR_FPR, cg());
               }
            else // numFloatArgs >= numFloatArgRegs
               {
               mref = getOutgoingArgumentMemRef(totalSize-argSize, argRegister, TR::InstOpCode::stfs, pushToMemory[argIndex++], 4);
               }
            numFloatArgs++;
            break;
         case TR::Double:
            argSize += 2*TR::Compiler->om.sizeofReferenceAddress();
            argRegister = pushDoubleArg(child);
            if (numFloatArgs < numFloatArgRegs)
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  tempRegister = cg()->allocateRegister(TR_FPR);
                  generateTrg1Src1Instruction(cg(), TR::InstOpCode::fmr, callNode, tempRegister, argRegister);
                  argRegister = tempRegister;
                  }
               if (numFloatArgs == 0 && resType.isFloatingPoint())
                  {
                  TR::Register *resultReg;
                  if (resType.getDataType() == TR::Float)
                     resultReg = cg()->allocateSinglePrecisionRegister();
                  else
                     resultReg = cg()->allocateRegister(TR_FPR);
                  dependencies->addPreCondition(argRegister, TR::RealRegister::fp0);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::fp0);
                  }
               else
                  addDependency(dependencies, argRegister, properties.getFloatArgumentRegister(numFloatArgs), TR_FPR, cg());
               }
            else // numFloatArgs >= numFloatArgRegs
               {
               mref = getOutgoingArgumentMemRef(totalSize-argSize, argRegister, TR::InstOpCode::stfd, pushToMemory[argIndex++], 8);
               }
            numFloatArgs++;
            break;
         }
      }

   if (!dependencies->searchPreConditionRegister(TR::RealRegister::gr11))
      addDependency(dependencies, NULL, TR::RealRegister::gr11, TR_GPR, cg());
   if (!dependencies->searchPreConditionRegister(TR::RealRegister::gr12))
      addDependency(dependencies, NULL, TR::RealRegister::gr12, TR_GPR, cg());

   for (int32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastGPR; ++i)
      {
      TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)i;
      if (properties.getPreserved(realReg))
         continue;
      if (realReg == specialArgReg)
         continue; // already added deps above.  No need to add them here.
      if (callSymbol->isComputed() && i == getProperties().getComputedCallTargetRegister())
         continue; // will be handled elsewhere
      if (!dependencies->searchPreConditionRegister(realReg))
         {
         if (realReg == properties.getIntegerArgumentRegister(0) && callNode->getDataType() == TR::Address)
            {
            dependencies->addPreCondition(cg()->allocateRegister(), TR::RealRegister::gr3);
            dependencies->addPostCondition(cg()->allocateCollectedReferenceRegister(), TR::RealRegister::gr3);
            }
         else
            {
            // Helper linkage preserves all registers that are not argument registers, so we don't need to spill them.
            if (linkage != TR_Helper)
               addDependency(dependencies, NULL, realReg, TR_GPR, cg());
            }
         }
      }

   TR_LiveRegisters *lr = cg()->getLiveRegisters(TR_FPR);
   bool liveFPR, liveVSX, liveVMX;

   liveFPR = (!lr || lr->getNumberOfLiveRegisters() > 0);

   lr = cg()->getLiveRegisters(TR_VSX_SCALAR);
   liveVSX = (!lr || lr->getNumberOfLiveRegisters() > 0);
   lr = cg()->getLiveRegisters(TR_VSX_VECTOR);
   liveVSX |= (!lr || lr->getNumberOfLiveRegisters() > 0);

   lr = cg()->getLiveRegisters(TR_VRF);
   liveVMX = (!lr || lr->getNumberOfLiveRegisters() > 0);

   if (liveVSX || liveFPR)
      {
      while (numFloatArgs < 8)
         {
         addDependency(dependencies, NULL, (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::fp0+numFloatArgs), TR_FPR, cg());
         numFloatArgs++;
         }

      int32_t theLastOne = liveVSX ? TR::RealRegister::LastVSR : TR::RealRegister::LastFPR;
      for (int32_t i = TR::RealRegister::fp8; i <= theLastOne; i++)
         if (!properties.getPreserved((TR::RealRegister::RegNum)i))
            {
            addDependency(dependencies, NULL, (TR::RealRegister::RegNum)i, (i>TR::RealRegister::LastFPR)?TR_VSX_SCALAR:TR_FPR, cg());
            }
      }
   else if (callNode->getType().isFloatingPoint())
      {
      //add return floating-point register dependency
      addDependency(dependencies, NULL, (TR::RealRegister::RegNum)getProperties().getFloatReturnRegister(), TR_FPR, cg());
      }

   if (!liveVSX && liveVMX)
      {
      for (int32_t i = TR::RealRegister::vr0; i <= TR::RealRegister::vr31; i++)
	 if (!properties.getPreserved((TR::RealRegister::RegNum)i))
	    addDependency(dependencies, NULL, (TR::RealRegister::RegNum)i, TR_VSX_SCALAR, cg());
      }

   // This is a performance hack. CCRs are rarely live across calls,
   // particularly helper calls, but because we create scratch virtual
   // regs and re-use them during evaluation they appear live.
   // This is often made worse by OOL code, the SRM, internal control flow.
   // The cost of saving/restoring CCRs in particular is very high and
   // fixing this problem is otherwise is not easy, hence this hack.
   // If you need to disable this hack, use this env var.
   static bool forcePreserveCCRs = feGetEnv("TR_ppcForcePreserveCCRsOnCalls") != NULL;
   if (linkage == TR_Private || forcePreserveCCRs)
      {
      lr = cg()->getLiveRegisters(TR_CCR);
      if(!lr || lr->getNumberOfLiveRegisters() > 0 )
         {
         for (int32_t i = TR::RealRegister::FirstCCR; i <= TR::RealRegister::LastCCR; i++)
            if (!properties.getPreserved((TR::RealRegister::RegNum)i))
               {
               addDependency(dependencies, NULL, (TR::RealRegister::RegNum)i, TR_CCR, cg());
               }
         }
      else
         {
         //add the FirstCCR if there is no live CCR available,
         //because on buildVirtualDispatch(), it gets the CCR dependency from search
         if (!properties.getPreserved(TR::RealRegister::FirstCCR))
            {
            addDependency(dependencies, NULL, TR::RealRegister::FirstCCR, TR_CCR, cg());
            }
         }
      }
   else
      {
      traceMsg(comp(), "Omitting CCR save/restore for helper calls\n");
      }

   if (memArgs > 0)
      {
      for (argIndex = 0; argIndex < memArgs; argIndex++)
         {
         TR::Register *aReg = pushToMemory[argIndex].argRegister;
         generateMemSrc1Instruction(cg(), pushToMemory[argIndex].opCode, callNode, pushToMemory[argIndex].argMemory, aReg);
         cg()->stopUsingRegister(aReg);
         }
      }

   return totalSize;
   }

static bool getProfiledCallSiteInfo(TR::CodeGenerator *cg, TR::Node *callNode, uint32_t maxStaticPICs, TR_ScratchList<TR::PPCPICItem> &values)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   if (comp->compileRelocatableCode())
      return false;

   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

   if (!methodSymbol->isVirtual() && !methodSymbol->isInterface())
      return false;

   TR_ValueProfileInfoManager *valueProfileInfo = TR_ValueProfileInfoManager::get(comp);
   if (!valueProfileInfo)
      return false;

   TR_AbstractInfo *info = (TR_AbstractInfo*)valueProfileInfo->getValueInfo(callNode->getByteCodeInfo(), comp);
   if (!info)
      return false;

   bool isAddressInfo = info->asAddressInfo() != NULL;
   if (!isAddressInfo && !info->asWarmCompilePICAddressInfo())
      return false;

   uint32_t totalFreq = info->getTotalFrequency();
   if (totalFreq == 0 || info->getTopProbability() < MIN_PROFILED_CALL_FREQUENCY)
      return false;

   TR_ScratchList<TR_ExtraAddressInfo> allValues(comp->trMemory());

   if (isAddressInfo)
      ((TR_AddressInfo *)info)->getSortedList(comp, (List<TR_ExtraAbstractInfo> *)&allValues);
   else
      ((TR_WarmCompilePICAddressInfo *)info)->getSortedList(comp, (List<TR_ExtraAbstractInfo> *)&allValues);

   TR_ResolvedMethod   *owningMethod = methodSymRef->getOwningMethod(comp);
   TR_OpaqueClassBlock *callSiteMethod;

   if (methodSymbol->isVirtual())
       callSiteMethod = methodSymRef->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod()->classOfMethod();

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
         TR_ASSERT(callSiteMethod, "Expecting valid callSiteMethod for virtual call");
         if (fej9->isInstanceOf(clazz, callSiteMethod, true, true) != TR_yes)
            continue;

         method = owningMethod->getResolvedVirtualMethod(comp, clazz, methodSymRef->getOffset());
         }
      else
         {
         TR_ASSERT(methodSymbol->isInterface(), "Expecting virtual or interface method");
         method = owningMethod->getResolvedInterfaceMethod(comp, clazz, methodSymRef->getCPIndex());
         }

      if (!method || method->isInterpreted())
         continue;

      values.add(new (comp->trStackMemory()) TR::PPCPICItem(clazz, method, freq));
      ++numStaticPics;
      }

   return numStaticPics > 0;
   }

static TR::Instruction* buildStaticPICCall(TR::CodeGenerator *cg, TR::Node *callNode, uintptrj_t profiledClass, TR_ResolvedMethod *profiledMethod,
                                             TR::Register *vftReg, TR::Register *tempReg, TR::Register *condReg, TR::LabelSymbol *missLabel,
                                             uint32_t regMapForGC)
   {
   TR::Compilation *comp = cg->comp();
   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::SymbolReference *profiledMethodSymRef = comp->getSymRefTab()->findOrCreateMethodSymbol(methodSymRef->getOwningMethodIndex(),
                                                                                             -1,
                                                                                             profiledMethod,
                                                                                             TR::MethodSymbol::Virtual);

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   loadAddressConstant(cg, callNode, profiledClass, tempReg, NULL, fej9->isUnloadAssumptionRequired((TR_OpaqueClassBlock *)profiledClass, comp->getCurrentMethod()));
   generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, callNode, condReg, vftReg, tempReg);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, callNode, missLabel, condReg);
   TR::Instruction *gcPoint = generateDepImmSymInstruction(cg, TR::InstOpCode::bl, callNode, (uintptrj_t)profiledMethod->startAddressForJittedMethod(),
                                                                                  new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0,0, cg->trMemory()), profiledMethodSymRef);
   gcPoint->PPCNeedsGCMap(regMapForGC);
   fej9->reserveTrampolineIfNecessary(comp, profiledMethodSymRef, false);
   return gcPoint;
   }

static void buildVirtualCall(TR::CodeGenerator *cg, TR::Node *callNode, TR::Register *vftReg, TR::Register *gr12, uint32_t regMapForGC)
   {
   int32_t offset = callNode->getSymbolReference()->getOffset();
   TR::Compilation* comp = cg->comp();

   // jitrt.dev/ppc/Recompilation.s is dependent on the code sequence
   // generated from here to the bctrl below!!
   // DO NOT MODIFY without also changing Recompilation.s!!
   if (offset < LOWER_IMMED || offset > UPPER_IMMED)
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, callNode, gr12, vftReg,
                                     (offset>>16)+((offset & (1<<15))?1:0) & 0x0000ffff);
      generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, callNode, gr12, new (cg->trHeapMemory()) TR::MemoryReference(gr12, ((offset & 0x0000ffff)<<16)>>16, TR::Compiler->om.sizeofReferenceAddress(), cg));
      }
   else
      {
      generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, callNode, gr12, new (cg->trHeapMemory()) TR::MemoryReference(vftReg, offset, TR::Compiler->om.sizeofReferenceAddress(), cg));
      }

   generateSrc1Instruction(cg, TR::InstOpCode::mtctr, callNode, gr12);
   TR::Instruction *gcPoint = generateInstruction(cg, TR::InstOpCode::bctrl, callNode);
   gcPoint->PPCNeedsGCMap(regMapForGC);
   }

static void buildInterfaceCall(TR::CodeGenerator *cg, TR::Node *callNode, TR::Register *vftReg, TR::Register *gr0, TR::Register *gr11, TR::Register *gr12, TR::Register *cr0, TR::PPCInterfaceCallSnippet *ifcSnippet, uint32_t regMapForGC)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

   // jitrt.dev/ppc/Recompilation.s is dependent on the code sequence
   // generated from here to the bctrl below!!
   // DO NOT MODIFY without also changing Recompilation.s!!
   if (TR::Compiler->target.is64Bit())
      {
      int32_t beginIndex = TR_PPCTableOfConstants::allocateChunk(1, cg);
      if (beginIndex != PTOC_FULL_INDEX)
         {
         beginIndex *= TR::Compiler->om.sizeofReferenceAddress();
         if (beginIndex < LOWER_IMMED || beginIndex > UPPER_IMMED)
            {
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, callNode, gr12, cg->getTOCBaseRegister(), HI_VALUE(beginIndex));
            generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, callNode, gr12, new (cg->trHeapMemory()) TR::MemoryReference(gr12, LO_VALUE(beginIndex), TR::Compiler->om.sizeofReferenceAddress(), cg));
            }
         else
            {
            generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, callNode, gr12, new (cg->trHeapMemory()) TR::MemoryReference(cg->getTOCBaseRegister(), beginIndex, TR::Compiler->om.sizeofReferenceAddress(), cg));
            }
         generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, callNode, gr11, new (cg->trHeapMemory()) TR::MemoryReference(gr12, 0, TR::Compiler->om.sizeofReferenceAddress(), cg));
         }
      else
         {
         TR::Instruction *q[4];
         fixedSeqMemAccess(cg, callNode, 0, q, gr11, gr12,TR::InstOpCode::Op_loadu, TR::Compiler->om.sizeofReferenceAddress(), NULL, gr11);
         ifcSnippet->setLowerInstruction(q[3]);
         ifcSnippet->setUpperInstruction(q[0]);
         }
      ifcSnippet->setTOCOffset(beginIndex);
      }
   else
      {
      ifcSnippet->setUpperInstruction(generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, callNode, gr12, 0));
      ifcSnippet->setLowerInstruction(generateTrg1MemInstruction(cg, TR::InstOpCode::lwzu, callNode, gr11, new (cg->trHeapMemory()) TR::MemoryReference(gr12, 0, 4, cg)));
      }
   TR::LabelSymbol *hitLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *snippetLabel = ifcSnippet->getSnippetLabel();
   generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, callNode, cr0, vftReg, gr11);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, callNode, hitLabel, cr0);

   generateTrg1MemInstruction(cg,TR::InstOpCode::Op_loadu, callNode, gr11, new (cg->trHeapMemory()) TR::MemoryReference(gr12, 2 * TR::Compiler->om.sizeofReferenceAddress(), TR::Compiler->om.sizeofReferenceAddress(), cg));
   generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, callNode, cr0, vftReg, gr11);

#ifdef INLINE_LASTITABLE_CHECK
   // Doing this check inline doesn't perform too well because it prevents the PIC cache slots from being populated with the best candidates.
   // This check is done in the _interfaceSlotsUnavailable helper instead.
   TR::LabelSymbol       *callLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR_SymbolReference  *methodSymRef = callNode->getSymbolReference();
   TR_ResolvedMethod   *owningMethod = methodSymRef->getOwningMethod(comp);
   TR_OpaqueClassBlock *interfaceClassOfMethod;
   uintptrj_t           itableIndex;

   interfaceClassOfMethod = owningMethod->getResolvedInterfaceMethod(methodSymRef->getCPIndex(), &itableIndex);

   TR_ASSERT(fej9->getITableEntryJitVTableOffset() <= UPPER_IMMED &&
             fej9->getITableEntryJitVTableOffset() >= LOWER_IMMED, "ITable offset for JIT is too large, prevents lastITable dispatch from being generated");

   if (!comp->getOption(TR_DisableLastITableCache) && interfaceClassOfMethod &&
       // Only do this if this offset can be loaded in a single instruction because this code may need to be
       // patched at runtime and we don't want to deal with too many variations
       fej9->getITableEntryJitVTableOffset() <= UPPER_IMMED &&
       fej9->getITableEntryJitVTableOffset() >= LOWER_IMMED)
      {
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, callNode, hitLabel, cr0);

      // Check if the lastITable belongs to the interface class we're using at this call site
      //
      generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, callNode, gr11,
                                 new (cg->trHeapMemory()) TR::MemoryReference(vftReg, fej9->getOffsetOfLastITableFromClassField(), TR::Compiler->om.sizeofReferenceAddress(), cg));
      // Load the interface class from the snippet rather than materializing it, again because we want to do this in a single instruction
      generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, callNode, gr12,
                                 new (cg->trHeapMemory()) TR::MemoryReference(gr12, -4 * TR::Compiler->om.sizeofReferenceAddress(), TR::Compiler->om.sizeofReferenceAddress(), cg));
      generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, callNode, gr0,
                                 new (cg->trHeapMemory()) TR::MemoryReference(gr11, fej9->getOffsetOfInterfaceClassFromITableField(), TR::Compiler->om.sizeofReferenceAddress(), cg));
      generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, callNode, cr0, gr0, gr12);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, callNode, snippetLabel, cr0);

      // lastITable belongs to the interface class we're using at this call site
      // Use it to look up the VFT offset and use that to make a virtual call
      //
      generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, callNode, gr12,
                                 new (cg->trHeapMemory()) TR::MemoryReference(gr11, fej9->convertITableIndexToOffset(itableIndex), TR::Compiler->om.sizeofReferenceAddress(), cg));
      loadConstant(cg, callNode, fej9->getITableEntryJitVTableOffset(), gr11);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, callNode, gr12, gr12, gr11);
      generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, callNode, gr11,
                                 new (cg->trHeapMemory()) TR::MemoryReference(vftReg, gr12, TR::Compiler->om.sizeofReferenceAddress(), cg));
      generateLabelInstruction(cg, TR::InstOpCode::b, callNode, callLabel);
      }
   else
#endif /* INLINE_LASTITABLE_CHECK */
      {
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, callNode, snippetLabel, cr0);
      }
   generateLabelInstruction(cg, TR::InstOpCode::label, callNode, hitLabel);
   generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, callNode, gr11, new (cg->trHeapMemory()) TR::MemoryReference(gr12, TR::Compiler->om.sizeofReferenceAddress(), TR::Compiler->om.sizeofReferenceAddress(), cg));
#ifdef INLINE_LASTITABLE_CHECK
   generateLabelInstruction(cg, TR::InstOpCode::label, callNode, callLabel);
#endif /* INLINE_LASTITABLE_CHECK */
   generateSrc1Instruction(cg, TR::InstOpCode::mtctr, callNode, gr11);
   TR::Instruction *gcPoint = generateInstruction(cg, TR::InstOpCode::bctrl, callNode);
   gcPoint->PPCNeedsGCMap(regMapForGC);
   ifcSnippet->gcMap().setGCRegisterMask(regMapForGC);
}

static TR::Register* evaluateUpToVftChild(TR::Node *callNode, TR::CodeGenerator *cg)
   {
   TR::Register *vftReg;
   uint32_t firstArgumentChild = callNode->getFirstArgumentIndex();
   for (uint32_t i = 0; i < firstArgumentChild; i++)
      {
      TR::Node *child = callNode->getChild(i);
      vftReg = cg->evaluate(child);
      cg->decReferenceCount(child);
      }
   return vftReg;
   }

void TR::PPCPrivateLinkage::buildVirtualDispatch(TR::Node                         *callNode,
                                                TR::RegisterDependencyConditions *dependencies,
                                                uint32_t                           sizeOfArguments)
   {
   TR::Register        *cr0 = dependencies->searchPreConditionRegister(TR::RealRegister::cr0);
   TR::Register        *gr0 = dependencies->searchPreConditionRegister(TR::RealRegister::gr0);
   TR::Register        *gr11 = dependencies->searchPreConditionRegister(TR::RealRegister::gr11);
   TR::Register        *gr12 = dependencies->searchPreConditionRegister(TR::RealRegister::gr12);
   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol   *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   TR::LabelSymbol     *doneLabel = generateLabelSymbol(cg());
   uint32_t             regMapForGC = getProperties().getPreservedRegisterMapForGC();
   void                *thunk = NULL;

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());

   // Computed calls
   //
   if (methodSymbol->isComputed())
      {
      TR::Register *vftReg = evaluateUpToVftChild(callNode, cg());
      // On 32-bit, we can just ignore the top 32 bits of the 64-bit target address
      if (vftReg->getRegisterPair())
         vftReg = vftReg->getLowOrder();
      addDependency(dependencies, vftReg, getProperties().getComputedCallTargetRegister(), TR_GPR, cg());

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
                                                 TR::PPCCallSnippet::generateVIThunk(fej9->getEquivalentVirtualCallNodeForDispatchVirtual(callNode, comp()), sizeOfArguments, cg()), comp());
               }
            }
         default:
            if (fej9->needsInvokeExactJ2IThunk(callNode, comp()))
               {
               comp()->getPersistentInfo()->getInvokeExactJ2IThunkTable()->addThunk(
                  TR::PPCCallSnippet::generateInvokeExactJ2IThunk(callNode, sizeOfArguments, cg(), methodSymbol->getMethod()->signatureChars()), fej9);
               }
            break;
         }

      TR::Register *targetAddress = vftReg;
      generateSrc1Instruction(cg(), TR::InstOpCode::mtctr, callNode, targetAddress);
      TR::Instruction *gcPoint = generateInstruction(cg(), TR::InstOpCode::bctrl, callNode);
      generateDepLabelInstruction(cg(), TR::InstOpCode::label, callNode, doneLabel, dependencies);

      gcPoint->PPCNeedsGCMap(regMapForGC);
      return;
      }

   // Virtual and interface calls
   //
   TR_ASSERT(methodSymbol->isVirtual() || methodSymbol->isInterface(), "Unexpected method type");

   thunk = fej9->getJ2IThunk(methodSymbol->getMethod(), comp());
   if (!thunk)
      thunk = fej9->setJ2IThunk(methodSymbol->getMethod(), TR::PPCCallSnippet::generateVIThunk(callNode, sizeOfArguments, cg()), comp());

   bool callIsSafe = methodSymRef != comp()->getSymRefTab()->findObjectNewInstanceImplSymbol();

   if (methodSymbol->isVirtual())
      {
      // Handle unresolved/AOT virtual calls first
      //
      if (methodSymRef->isUnresolved() || cg()->comp()->compileRelocatableCode())
         {
         TR::Register *vftReg = evaluateUpToVftChild(callNode, cg());
         addDependency(dependencies, vftReg, TR::RealRegister::NoReg, TR_GPR, cg());

         TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg());
         TR::Snippet  *snippet = new (trHeapMemory()) TR::PPCVirtualUnresolvedSnippet(cg(), callNode, snippetLabel, sizeOfArguments, doneLabel);
         cg()->addSnippet(snippet);

         generateTrg1Src1Instruction(cg(), TR::InstOpCode::mr, callNode, gr12, vftReg);

         // These two instructions will be ultimately modified to load the
         // method pointer. The branch should be a no-op if the offset turns
         // out to be within range (for most cases).
         generateLabelInstruction(cg(), TR::InstOpCode::b, callNode, snippetLabel);
         generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, callNode, gr12, new (trHeapMemory()) TR::MemoryReference(gr12, 0, TR::Compiler->om.sizeofReferenceAddress(), cg()));

         generateSrc1Instruction(cg(), TR::InstOpCode::mtctr, callNode, gr12);
         TR::Instruction *gcPoint = generateInstruction(cg(), TR::InstOpCode::bctrl, callNode);
         generateDepLabelInstruction(cg(), TR::InstOpCode::label, callNode, doneLabel, dependencies);

         gcPoint->PPCNeedsGCMap(regMapForGC);
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
               virtualGuard = TR_VirtualGuard::createGuardedDevirtualizationGuard(TR_NonoverriddenGuard, comp(), callNode);
               }
            else
               {
               TR_DevirtualizedCallInfo *devirtualizedCallInfo = comp()->findDevirtualizedCall(callNode);
               TR_OpaqueClassBlock      *refinedThisClass = devirtualizedCallInfo ? devirtualizedCallInfo->_thisType : NULL;
               TR_OpaqueClassBlock      *thisClass = refinedThisClass ? refinedThisClass : resolvedMethod->containingClass();

               TR_PersistentCHTable     *chTable = comp()->getPersistentInfo()->getPersistentCHTable();
               if (thisClass && TR::Compiler->cls.isAbstractClass(comp(), thisClass))
                  {
                  TR_ResolvedMethod *calleeMethod = chTable->findSingleAbstractImplementer(thisClass, methodSymRef->getOffset(), methodSymRef->getOwningMethod(comp()), comp());
                  if (calleeMethod &&
                      ((calleeMethod->isSameMethod(comp()->getCurrentMethod()) && !comp()->isDLT()) ||
                       !calleeMethod->isInterpreted() ||
                       calleeMethod->isJITInternalNative()))
                     {
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
                      ((calleeMethod->isSameMethod(comp()->getCurrentMethod()) && !comp()->isDLT()) ||
                       !calleeMethod->isInterpreted() ||
                       calleeMethod->isJITInternalNative()))
                     {
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
               TR::Node     *vftNode = callNode->getFirstChild();
               TR::Register *vftReg = NULL;

               // We prefer to evaluate the VFT child in the out of line path if possible, but if we can't do it then, do it now instead
               if (vftNode->getReferenceCount() > 1 || vftNode->getRegister())
                  {
                  vftReg = evaluateUpToVftChild(callNode, cg());
                  addDependency(dependencies, vftReg, TR::RealRegister::NoReg, TR_GPR, cg());
                  }
               else
                  {
                  // If we push evaluation of the VFT node off to the outlined code section we have to make sure that any of it's children
                  // that are needed later are still evaluated. We could, in theory skip evaluating children with refcount == 1 for the same
                  // reason as above, but would have to then look at the child's children, and so on, so just evaluate the VFT's children here and now.
                  for (uint32_t i = 0; i < vftNode->getNumChildren(); i++)
                     cg()->evaluate(vftNode->getChild(i));
                  }

               TR::LabelSymbol *virtualCallLabel = generateLabelSymbol(cg());
               generateVirtualGuardNOPInstruction(cg(), callNode, virtualGuard->addNOPSite(), new (trHeapMemory()) TR::RegisterDependencyConditions(0, 0, trMemory()), virtualCallLabel);
               if (comp()->getOption(TR_EnableHCR))
                  {
                  TR_VirtualGuard *HCRGuard = TR_VirtualGuard::createGuardedDevirtualizationGuard(TR_HCRGuard, comp(), callNode);
                  generateVirtualGuardNOPInstruction(cg(), callNode, HCRGuard->addNOPSite(), new (trHeapMemory()) TR::RegisterDependencyConditions(0, 0, trMemory()), virtualCallLabel);
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
               uintptrj_t methodAddress = resolvedMethod->isSameMethod(comp()->getCurrentMethod()) && !comp()->isDLT() ? 0 : (uintptrj_t)resolvedMethod->startAddressForJittedMethod();
               TR::Instruction *gcPoint = generateDepImmSymInstruction(cg(), TR::InstOpCode::bl, callNode, methodAddress,
                                                                                              new (trHeapMemory()) TR::RegisterDependencyConditions(0, 0, trMemory()), methodSymRef);
               generateDepLabelInstruction(cg(), TR::InstOpCode::label, callNode, doneLabel, dependencies);
               gcPoint->PPCNeedsGCMap(regMapForGC);
               fej9->reserveTrampolineIfNecessary(comp(), methodSymRef, false);

               // Out of line virtual call
               //
               TR_PPCOutOfLineCodeSection *virtualCallOOL = new (trHeapMemory()) TR_PPCOutOfLineCodeSection(virtualCallLabel, doneLabel, cg());

               virtualCallOOL->swapInstructionListsWithCompilation();
               TR::Instruction *OOLLabelInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, virtualCallLabel);

               // XXX: Temporary fix, OOL instruction stream does not pick up live locals or monitors correctly.
               TR_ASSERT(!OOLLabelInstr->getLiveLocals() && !OOLLabelInstr->getLiveMonitors(), "Expecting first OOL instruction to not have live locals/monitors info");
               OOLLabelInstr->setLiveLocals(gcPoint->getLiveLocals());
               OOLLabelInstr->setLiveMonitors(gcPoint->getLiveMonitors());

               TR::RegisterDependencyConditions *outlinedCallDeps;
               if (!vftReg)
                  {
                  vftReg = evaluateUpToVftChild(callNode, cg());
                  outlinedCallDeps = new (trHeapMemory()) TR::RegisterDependencyConditions(1, 1, trMemory());
                  // Normally we don't care which reg the VFT gets, but in this case we don't want to undo the assignments done by the main deps so we choose a reg that we know will not be needed
                  addDependency(outlinedCallDeps, vftReg, TR::RealRegister::gr11, TR_GPR, cg());
                  }
               else
                  outlinedCallDeps = new (trHeapMemory()) TR::RegisterDependencyConditions(0, 0, trMemory());


               buildVirtualCall(cg(), callNode, vftReg, gr12, regMapForGC);

               TR::LabelSymbol *doneOOLLabel = generateLabelSymbol(cg());
               generateDepLabelInstruction(cg(), TR::InstOpCode::label, callNode, doneOOLLabel, outlinedCallDeps);
               generateLabelInstruction(cg(), TR::InstOpCode::b, callNode, doneLabel);
               virtualCallOOL->swapInstructionListsWithCompilation();
               cg()->getPPCOutOfLineCodeSectionList().push_front(virtualCallOOL);

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
   TR::Register *vftReg = evaluateUpToVftChild(callNode, cg());
   addDependency(dependencies, vftReg, TR::RealRegister::NoReg, TR_GPR, cg());

   if (callIsSafe && !callNode->isTheVirtualCallNodeForAGuardedInlinedCall() && !comp()->getOption(TR_DisableInterpreterProfiling))
      {
      static char     *maxVirtualStaticPICsString = feGetEnv("TR_ppcMaxVirtualStaticPICs");
      static uint32_t  maxVirtualStaticPICs = maxVirtualStaticPICsString ? atoi(maxVirtualStaticPICsString) : 1;
      static char     *maxInterfaceStaticPICsString = feGetEnv("TR_ppcMaxInterfaceStaticPICs");
      static uint32_t  maxInterfaceStaticPICs = maxInterfaceStaticPICsString ? atoi(maxInterfaceStaticPICsString) : 1;

      TR_ScratchList<TR::PPCPICItem> values(cg()->trMemory());
      const uint32_t maxStaticPICs = methodSymbol->isInterface() ? maxInterfaceStaticPICs : maxVirtualStaticPICs;

      if (getProfiledCallSiteInfo(cg(), callNode, maxStaticPICs, values))
         {
         ListIterator<TR::PPCPICItem>  i(&values);
         TR::PPCPICItem               *pic = i.getFirst();

         // If this value is dominant, optimize exclusively for it
         if (pic->_frequency > MAX_PROFILED_CALL_FREQUENCY)
            {
            TR::LabelSymbol *slowCallLabel = generateLabelSymbol(cg());

            TR::Instruction *gcPoint = buildStaticPICCall(cg(), callNode, (uintptrj_t)pic->_clazz, pic->_method,
                                                            vftReg, gr11, cr0, slowCallLabel, regMapForGC);
            generateDepLabelInstruction(cg(), TR::InstOpCode::label, callNode, doneLabel, dependencies);

            // Out of line virtual/interface call
            //
            TR_PPCOutOfLineCodeSection *slowCallOOL = new (trHeapMemory()) TR_PPCOutOfLineCodeSection(slowCallLabel, doneLabel, cg());

            slowCallOOL->swapInstructionListsWithCompilation();
            TR::Instruction *OOLLabelInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, slowCallLabel);

            // XXX: Temporary fix, OOL instruction stream does not pick up live locals or monitors correctly.
            TR_ASSERT(!OOLLabelInstr->getLiveLocals() && !OOLLabelInstr->getLiveMonitors(), "Expecting first OOL instruction to not have live locals/monitors info");
            OOLLabelInstr->setLiveLocals(gcPoint->getLiveLocals());
            OOLLabelInstr->setLiveMonitors(gcPoint->getLiveMonitors());

            TR::LabelSymbol *doneOOLLabel = generateLabelSymbol(cg());

            if (methodSymbol->isInterface())
               {
               TR::LabelSymbol              *ifcSnippetLabel = generateLabelSymbol(cg());
               TR::PPCInterfaceCallSnippet *ifcSnippet = new (trHeapMemory()) TR::PPCInterfaceCallSnippet(cg(), callNode, ifcSnippetLabel, sizeOfArguments, doneOOLLabel, (uint8_t *)thunk);

               cg()->addSnippet(ifcSnippet);
               buildInterfaceCall(cg(), callNode, vftReg, gr0, gr11, gr12, cr0, ifcSnippet, regMapForGC);
               }
            else
               {
               buildVirtualCall(cg(), callNode, vftReg, gr12, regMapForGC);
               }

            generateDepLabelInstruction(cg(), TR::InstOpCode::label, callNode, doneOOLLabel, new (trHeapMemory()) TR::RegisterDependencyConditions(0, 0, trMemory()));
            generateLabelInstruction(cg(), TR::InstOpCode::b, callNode, doneLabel);
            slowCallOOL->swapInstructionListsWithCompilation();
            cg()->getPPCOutOfLineCodeSectionList().push_front(slowCallOOL);

            return;
            }
         else
            {
            // Build multiple static PIC calls
            while (pic)
               {
               TR::LabelSymbol *nextLabel = generateLabelSymbol(cg());

               buildStaticPICCall(cg(), callNode, (uintptrj_t)pic->_clazz, pic->_method,
                                  vftReg, gr11, cr0, nextLabel, regMapForGC);
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
      TR::LabelSymbol              *ifcSnippetLabel = generateLabelSymbol(cg());
      TR::PPCInterfaceCallSnippet *ifcSnippet = new (trHeapMemory()) TR::PPCInterfaceCallSnippet(cg(), callNode, ifcSnippetLabel, sizeOfArguments, doneLabel, (uint8_t *)thunk);

      cg()->addSnippet(ifcSnippet);
      buildInterfaceCall(cg(), callNode, vftReg, gr0, gr11, gr12, cr0, ifcSnippet, regMapForGC);
      }
   else
      {
      buildVirtualCall(cg(), callNode, vftReg, gr12, regMapForGC);
      }
   generateDepLabelInstruction(cg(), TR::InstOpCode::label, callNode, doneLabel, dependencies);
   }

void inlineCharacterIsMethod(TR::Node *node, TR::MethodSymbol* methodSymbol, TR::CodeGenerator *cg, TR::LabelSymbol *&doneLabel)
   {
   TR::Compilation * comp = cg->comp();

   TR::LabelSymbol *inRangeLabel = generateLabelSymbol(cg);
   TR::Register *cnd0Reg = cg->allocateRegister(TR_CCR);
   TR::Register *cnd1Reg = cg->allocateRegister(TR_CCR);
   TR::Register *cnd2Reg = cg->allocateRegister(TR_CCR);
   TR::Register *srcReg = cg->evaluate(node->getFirstChild());
   TR::Register *rangeReg = cg->allocateRegister(TR_GPR);
   TR::Register *returnRegister = cg->allocateRegister(TR_GPR);
   TR::Register *tmpReg = cg->allocateRegister(TR_GPR);

   TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(6, 6, cg->trMemory());
   addDependency(dependencies, srcReg, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(dependencies, rangeReg, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(dependencies, returnRegister, TR::RealRegister::gr3, TR_GPR, cg);
   addDependency(dependencies, cnd0Reg, TR::RealRegister::cr0, TR_CCR, cg);
   addDependency(dependencies, cnd1Reg, TR::RealRegister::NoReg, TR_CCR, cg);
   addDependency(dependencies, cnd2Reg, TR::RealRegister::NoReg, TR_CCR, cg);

   int32_t imm = (TR::RealRegister::CRCC_GT << TR::RealRegister::pos_RT | TR::RealRegister::CRCC_GT << TR::RealRegister::pos_RA | TR::RealRegister::CRCC_GT << TR::RealRegister::pos_RB);

   int64_t mask = 0xFFFF00;
   if (methodSymbol->getRecognizedMethod() == TR::java_lang_Character_isLetter ||
       methodSymbol->getRecognizedMethod() == TR::java_lang_Character_isAlphabetic )
      {
      mask = 0xFFFF80; // mask for ASCII
      }
   generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm_r, node, tmpReg, srcReg, 0, mask); // set cnd0Reg(cr0)

   switch(methodSymbol->getRecognizedMethod())
      {
      case TR::java_lang_Character_isDigit:
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, rangeReg, 0x3930);
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::cmprb, node, cnd1Reg, srcReg, rangeReg, 0);
         break;
      case TR::java_lang_Character_isAlphabetic:
      case TR::java_lang_Character_isLetter:
         //ASCII only
         generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, rangeReg, 0x7A61);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, rangeReg, rangeReg, 0x5A41);
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::cmprb, node, cnd1Reg, srcReg, rangeReg, 1);
         break;
      case TR::java_lang_Character_isWhitespace:
         generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, rangeReg, 0x0D09);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, rangeReg, rangeReg, 0x201C);
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::cmprb, node, cnd1Reg, srcReg, rangeReg, 1);
         break;
      case TR::java_lang_Character_isUpperCase:
         generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, rangeReg, 0x5A41);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, rangeReg, rangeReg, 0xD6C0);
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::cmprb, node, cnd1Reg, srcReg, rangeReg, 1);
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, rangeReg, 0xDED8);
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::cmprb, node, cnd2Reg, srcReg, rangeReg, 0);
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::cror, node, cnd1Reg, cnd2Reg, cnd1Reg, imm);
         break;
      case TR::java_lang_Character_isLowerCase:
         loadConstant(cg, node, (int32_t)0x7A61F6DF, rangeReg);
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::cmprb, node, cnd1Reg, srcReg, rangeReg, 1);
         loadConstant(cg, node, (int32_t)0xFFFFFFF8, rangeReg);
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::cmprb, node, cnd2Reg, srcReg, rangeReg, 0);
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::cror, node, cnd1Reg, cnd2Reg, cnd1Reg, imm);
         loadConstant(cg, node, (int64_t)0xFFFFFFFFFFAAB5BA, rangeReg);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::cmpeqb, node, cnd2Reg, srcReg, rangeReg);
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::cror, node, cnd1Reg, cnd2Reg, cnd1Reg, imm);
         break;
      }
   generateTrg1Src1Instruction(cg, TR::InstOpCode::setb, node, returnRegister, cnd1Reg);

   generateDepConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, cnd0Reg, dependencies);

   dependencies->stopUsingDepRegs(cg);
   cg->stopUsingRegister(returnRegister);
   cg->stopUsingRegister(cnd0Reg);
   cg->stopUsingRegister(cnd1Reg);
   cg->stopUsingRegister(cnd2Reg);
   cg->stopUsingRegister(srcReg);
   cg->stopUsingRegister(rangeReg);
   cg->stopUsingRegister(tmpReg);
   }

void TR::PPCPrivateLinkage::buildDirectCall(TR::Node *callNode,
                                           TR::SymbolReference *callSymRef,
                                           TR::RegisterDependencyConditions *dependencies,
                                           const TR::PPCLinkageProperties &pp,
                                           int32_t argSize)
   {
   TR::Instruction *gcPoint;
   TR::MethodSymbol *callSymbol    = callSymRef->getSymbol()->castToMethodSymbol();
   TR::ResolvedMethodSymbol *sym   = callSymbol->getResolvedMethodSymbol();
   TR_ResolvedMethod *vmm       = (sym==NULL)?NULL:sym->getResolvedMethod();
   bool myself = 
      (vmm!=NULL && vmm->isSameMethod(comp()->getCurrentMethod()) && !comp()->isDLT()) ?
      true : false;

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());

   if (callSymRef->getReferenceNumber() >= TR_PPCnumRuntimeHelpers)
      fej9->reserveTrampolineIfNecessary(comp(), callSymRef, false);

   if ((callSymbol->isJITInternalNative() ||
        (!callSymRef->isUnresolved() && !callSymbol->isInterpreted() && ((comp()->compileRelocatableCode() && callSymbol->isHelper()) || !comp()->compileRelocatableCode()))))
      {
      gcPoint = generateDepImmSymInstruction(cg(), TR::InstOpCode::bl, callNode,
                                                                  myself?0:(uintptrj_t)callSymbol->getMethodAddress(),
                                                                  dependencies, callSymRef?callSymRef:callNode->getSymbolReference());
      }
   else
      {
      TR::LabelSymbol *label   = generateLabelSymbol(cg());
      TR::Snippet     *snippet;

      if (callSymRef->isUnresolved() || comp()->compileRelocatableCode())
         {
         snippet = new (trHeapMemory()) TR::PPCUnresolvedCallSnippet(cg(), callNode, label, argSize);
         }
      else
         {
         snippet = new (trHeapMemory()) TR::PPCCallSnippet(cg(), callNode, label, callSymRef, argSize);
         snippet->gcMap().setGCRegisterMask(pp.getPreservedRegisterMapForGC());
         }

      cg()->addSnippet(snippet);
      gcPoint = generateDepImmSymInstruction(cg(), TR::InstOpCode::bl, callNode,
                                                                  0,
                                                                  dependencies,
                                                                  new (trHeapMemory()) TR::SymbolReference(comp()->getSymRefTab(), label),
                                                                  snippet);

      // Nop is necessary due to confusion when resolving shared slots at a transition
      if (callSymRef == cg()->symRefTab()->element(TR_induceOSRAtCurrentPC))
         cg()->generateNop(callNode);
      }

   // Helper linkage needs to cover volatiles in the GC map at this point because, unlike private linkage calls,
   // volatiles are not spilled prior to the call.
   gcPoint->PPCNeedsGCMap(callSymbol->getLinkageConvention() == TR_Helper ? 0xffffffff : pp.getPreservedRegisterMapForGC());
   }


TR::Register *TR::PPCPrivateLinkage::buildDirectDispatch(TR::Node *callNode)
   {
   TR::SymbolReference *callSymRef = callNode->getSymbolReference();

   const TR::PPCLinkageProperties &pp = getProperties();
   TR::RegisterDependencyConditions *dependencies =
      new (trHeapMemory()) TR::RegisterDependencyConditions(
         pp.getNumberOfDependencyGPRegisters(),
         pp.getNumberOfDependencyGPRegisters(), trMemory());

   TR::Register    *returnRegister=NULL;
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg());

   // SG - start
   int32_t           argSize = buildArgs(callNode, dependencies);
   bool inlinedCharacterIsMethod = false;
   if (TR::Compiler->target.cpu.id() >= TR_PPCp9 && TR::Compiler->target.is64Bit())
      {
      switch(callNode->getSymbol()->castToMethodSymbol()->getRecognizedMethod())
         {
         case TR::java_lang_Character_isDigit:
         case TR::java_lang_Character_isLetter:
         case TR::java_lang_Character_isUpperCase:
         case TR::java_lang_Character_isLowerCase:
         case TR::java_lang_Character_isWhitespace:
         case TR::java_lang_Character_isAlphabetic:
            inlinedCharacterIsMethod = true;
            inlineCharacterIsMethod(callNode, callNode->getSymbol()->castToMethodSymbol(), cg(), doneLabel);
            break;
         }
      }

   buildDirectCall(callNode, callSymRef, dependencies, pp, argSize);
   // SG - end

   cg()->machine()->setLinkRegisterKilled(true);
   cg()->setHasCall();

   TR::Register *lowReg=NULL, *highReg=NULL;
   switch(callNode->getOpCodeValue())
      {
      case TR::icall:
      case TR::acall:
         returnRegister = dependencies->searchPostConditionRegister(
                             pp.getIntegerReturnRegister());
         break;
      case TR::lcall:
         {
         if (TR::Compiler->target.is64Bit())
            returnRegister = dependencies->searchPostConditionRegister(
                                pp.getLongReturnRegister());
         else
            {
            lowReg  = dependencies->searchPostConditionRegister(
                         pp.getLongLowReturnRegister());
            highReg = dependencies->searchPostConditionRegister(
                         pp.getLongHighReturnRegister());

            returnRegister = cg()->allocateRegisterPair(lowReg, highReg);
            }
         }
         break;
      case TR::fcall:
      case TR::dcall:
         returnRegister = dependencies->searchPostConditionRegister(
                             pp.getFloatReturnRegister());
         if (callNode->getDataType() == TR::Float)
            returnRegister->setIsSinglePrecision();
         break;
      case TR::call:
         returnRegister = NULL;
         break;
      default:
         returnRegister = NULL;
         TR_ASSERT(0, "Unknown direct call Opcode.");
      }

   if (inlinedCharacterIsMethod)
      {
      generateDepLabelInstruction(cg(), TR::InstOpCode::label, callNode, doneLabel, dependencies->cloneAndFix(cg()));
      }

   callNode->setRegister(returnRegister);

   cg()->freeAndResetTransientLongs();
   dependencies->stopUsingDepRegs(cg(), lowReg==NULL?returnRegister:highReg, lowReg);
   return(returnRegister);
   }

TR::Register *TR::PPCPrivateLinkage::buildIndirectDispatch(TR::Node *callNode)
   {
   const TR::PPCLinkageProperties &pp = getProperties();
   TR::RegisterDependencyConditions *dependencies =
      new (trHeapMemory()) TR::RegisterDependencyConditions(
         pp.getNumberOfDependencyGPRegisters()+1,
         pp.getNumberOfDependencyGPRegisters()+1, trMemory());

   int32_t             argSize = buildArgs(callNode, dependencies);
   TR::Register        *returnRegister;

   buildVirtualDispatch(callNode, dependencies, argSize);
   cg()->machine()->setLinkRegisterKilled(true);
   cg()->setHasCall();

   TR::Register *lowReg=NULL, *highReg;
   switch(callNode->getOpCodeValue())
      {
      case TR::icalli:
      case TR::acalli:
         returnRegister = dependencies->searchPostConditionRegister(
                             pp.getIntegerReturnRegister());
         break;
      case TR::lcalli:
         {
         if (TR::Compiler->target.is64Bit())
            returnRegister = dependencies->searchPostConditionRegister(
                                pp.getLongReturnRegister());
         else
            {
            lowReg  = dependencies->searchPostConditionRegister(
                         pp.getLongLowReturnRegister());
            highReg = dependencies->searchPostConditionRegister(
                         pp.getLongHighReturnRegister());

            returnRegister = cg()->allocateRegisterPair(lowReg, highReg);
            }
         }
         break;
      case TR::fcalli:
      case TR::dcalli:
         returnRegister = dependencies->searchPostConditionRegister(
                             pp.getFloatReturnRegister());
         if (callNode->getDataType() == TR::Float)
            returnRegister->setIsSinglePrecision();
         break;
      case TR::calli:
         returnRegister = NULL;
         break;
      default:
         returnRegister = NULL;
         TR_ASSERT(0, "Unknown indirect call Opcode.");
      }

   callNode->setRegister(returnRegister);

   cg()->freeAndResetTransientLongs();
   dependencies->stopUsingDepRegs(cg(), lowReg==NULL?returnRegister:highReg, lowReg);
   return(returnRegister);
   }

TR::Register *TR::PPCPrivateLinkage::buildalloca(TR::Node *BIFCallNode)
   {
   TR_ASSERT(0,"PPCPrivateLinkage does not support alloca.\n");
   return NULL;
   }

int32_t TR::PPCHelperLinkage::buildArgs(TR::Node *callNode,
                                       TR::RegisterDependencyConditions *dependencies)
   {
   return buildPrivateLinkageArgs(callNode, dependencies, _helperLinkage);
   }

TR::MemoryReference *TR::PPCPrivateLinkage::getOutgoingArgumentMemRef(int32_t argSize, TR::Register *argReg, TR::InstOpCode::Mnemonic opCode, TR::PPCMemoryArgument &memArg, uint32_t length, const TR::PPCLinkageProperties& properties)
   {
   TR::Machine *machine = cg()->machine();

   TR::MemoryReference *result=new (trHeapMemory()) TR::MemoryReference(machine->getPPCRealRegister(properties.getNormalStackPointerRegister()),
                                        argSize+properties.getOffsetToFirstParm(), length, cg());
   memArg.argRegister = argReg;
   memArg.argMemory = result;
   memArg.opCode = opCode;
   return(result);
   }
