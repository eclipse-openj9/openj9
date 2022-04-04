/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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

#include "codegen/S390PrivateLinkage.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Snippet.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/VirtualGuard.hpp"
#include "env/CHTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/J2IThunk.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/VMJ9.h"
#include "env/jittypes.h"
#include "env/j9method.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/InterferenceGraph.hpp"
#include "z/codegen/OpMemToMem.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390HelperCallSnippet.hpp"
#include "z/codegen/S390J9CallSnippet.hpp"
#include "z/codegen/S390StackCheckFailureSnippet.hpp"
#include "z/codegen/SystemLinkage.hpp"
#include "z/codegen/SystemLinkagezOS.hpp"
#include "runtime/J9Profiler.hpp"
#include "runtime/J9ValueProfiler.hpp"

#define MIN_PROFILED_CALL_FREQUENCY (.075f)

////////////////////////////////////////////////////////////////////////////////
// J9::Z::PrivateLinkage for J9
////////////////////////////////////////////////////////////////////////////////
J9::Z::PrivateLinkage::PrivateLinkage(TR::CodeGenerator * codeGen,TR_LinkageConventions lc)
   : J9::PrivateLinkage(codeGen)
   {
   setLinkageType(lc);

   // linkage properties
   setProperty(SplitLongParm);
   setProperty(TwoStackSlotsForLongAndDouble);

   //Preserved Registers

   setRegisterFlag(TR::RealRegister::GPR5, Preserved);
   setRegisterFlag(TR::RealRegister::GPR6, Preserved);
   setRegisterFlag(TR::RealRegister::GPR7, Preserved);
   setRegisterFlag(TR::RealRegister::GPR8, Preserved);
   setRegisterFlag(TR::RealRegister::GPR9, Preserved);
   setRegisterFlag(TR::RealRegister::GPR10, Preserved);
   setRegisterFlag(TR::RealRegister::GPR11, Preserved);
   setRegisterFlag(TR::RealRegister::GPR12, Preserved);
   setRegisterFlag(TR::RealRegister::GPR13, Preserved);

#if defined(ENABLE_PRESERVED_FPRS)
   setRegisterFlag(TR::RealRegister::FPR8, Preserved);
   setRegisterFlag(TR::RealRegister::FPR9, Preserved);
   setRegisterFlag(TR::RealRegister::FPR10, Preserved);
   setRegisterFlag(TR::RealRegister::FPR11, Preserved);
   setRegisterFlag(TR::RealRegister::FPR12, Preserved);
   setRegisterFlag(TR::RealRegister::FPR13, Preserved);
   setRegisterFlag(TR::RealRegister::FPR14, Preserved);
   setRegisterFlag(TR::RealRegister::FPR15, Preserved);
#endif

   setIntegerReturnRegister (TR::RealRegister::GPR2 );
   setLongLowReturnRegister (TR::RealRegister::GPR3 );
   setLongHighReturnRegister(TR::RealRegister::GPR2 );
   setLongReturnRegister    (TR::RealRegister::GPR2 );
   setFloatReturnRegister   (TR::RealRegister::FPR0 );
   setDoubleReturnRegister  (TR::RealRegister::FPR0 );
   setLongDoubleReturnRegister0  (TR::RealRegister::FPR0 );
   setLongDoubleReturnRegister2  (TR::RealRegister::FPR2 );
   setLongDoubleReturnRegister4  (TR::RealRegister::FPR4 );
   setLongDoubleReturnRegister6  (TR::RealRegister::FPR6 );

   const bool enableVectorLinkage = codeGen->getSupportsVectorRegisters();
   if (enableVectorLinkage) setVectorReturnRegister(TR::RealRegister::VRF24);

   setStackPointerRegister  (TR::RealRegister::GPR5 );
   setEntryPointRegister    (comp()->target().isLinux() ? TR::RealRegister::GPR4 : TR::RealRegister::GPR15);
   setReturnAddressRegister (TR::RealRegister::GPR14);

   setVTableIndexArgumentRegister (TR::RealRegister::GPR0);
   setJ9MethodArgumentRegister    (TR::RealRegister::GPR1);

   setLitPoolRegister       (TR::RealRegister::GPR6  );
   setMethodMetaDataRegister(TR::RealRegister::GPR13 );

   setIntegerArgumentRegister(0, TR::RealRegister::GPR1);
   setIntegerArgumentRegister(1, TR::RealRegister::GPR2);
   setIntegerArgumentRegister(2, TR::RealRegister::GPR3);
   setNumIntegerArgumentRegisters(3);

   setFloatArgumentRegister(0, TR::RealRegister::FPR0);
   setFloatArgumentRegister(1, TR::RealRegister::FPR2);
   setFloatArgumentRegister(2, TR::RealRegister::FPR4);
   setFloatArgumentRegister(3, TR::RealRegister::FPR6);
   setNumFloatArgumentRegisters(4);

   if (enableVectorLinkage)
      {
      int vecIndex = 0;
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF25);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF26);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF27);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF28);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF29);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF30);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF31);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF24);
      setNumVectorArgumentRegisters(vecIndex);
      }

   setOffsetToFirstLocal  (comp()->target().is64Bit() ? -8 : -4);
   setOffsetToRegSaveArea (0);
   setOffsetToLongDispSlot(0);
   setOffsetToFirstParm   (0);
   int32_t numDeps = 30;

   if (codeGen->getSupportsVectorRegisters())
      numDeps += 32; //need to kill VRFs

   setNumberOfDependencyGPRegisters(numDeps);

   setPreservedRegisterMapForGC(0x00001fc0);
   setLargestOutgoingArgumentAreaSize(0);
   }

////////////////////////////////////////////////////////////////////////////////
// J9::Z::PrivateLinkage::initS390RealRegisterLinkage - initialize the state
//    of real register for register allocator
////////////////////////////////////////////////////////////////////////////////
void
J9::Z::PrivateLinkage::initS390RealRegisterLinkage()
   {
   TR::RealRegister * sspReal = getSystemStackPointerRealRegister();
   TR::RealRegister * spReal  = getStackPointerRealRegister();
   TR::RealRegister * mdReal  = getMethodMetaDataRealRegister();
   int32_t icount, ret_count = 0;

   // Lock all the dedicated registers
   bool freeingSSPDisabled = true;

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());

   if (cg()->supportsJITFreeSystemStackPointer())
      freeingSSPDisabled = false;

   if (freeingSSPDisabled)
      {
      sspReal->setState(TR::RealRegister::Locked);
      sspReal->setAssignedRegister(sspReal);
      sspReal->setHasBeenAssignedInMethod(true);
      }

   // Java Stack  pointer
   spReal->setState(TR::RealRegister::Locked);
   spReal->setAssignedRegister(spReal);
   spReal->setHasBeenAssignedInMethod(true);

   // meta data register
   mdReal->setState(TR::RealRegister::Locked);
   mdReal->setAssignedRegister(mdReal);
   mdReal->setHasBeenAssignedInMethod(true);

   // set register weight
   for (icount = TR::RealRegister::FirstGPR; icount <= TR::RealRegister::GPR3; icount++)
      {
      int32_t weight;
      if (getIntegerReturn((TR::RealRegister::RegNum) icount))
         {
         weight = ++ret_count;
         }
      else
         {
         weight = icount;
         }
      cg()->machine()->getRealRegister((TR::RealRegister::RegNum) icount)->setWeight(weight);
      }

   for (icount = TR::RealRegister::GPR4; icount >= TR::RealRegister::LastAssignableGPR; icount++)
      {
      cg()->machine()->getRealRegister((TR::RealRegister::RegNum) icount)->setWeight(0xf000 + icount);
      }
   }

void J9::Z::PrivateLinkage::alignLocalsOffset(uint32_t &stackIndex, uint32_t localObjectAlignment)
   {
   if (stackIndex % localObjectAlignment != 0)
      {
      uint32_t stackIndexBeforeAlignment = stackIndex;

      // TODO: Is the negation here necessary?
      stackIndex = -((-stackIndex + (localObjectAlignment - 1)) & ~(localObjectAlignment - 1));

      TR::GCStackAtlas *atlas = cg()->getStackAtlas();

      atlas->setNumberOfSlotsMapped(atlas->getNumberOfSlotsMapped() + ((stackIndexBeforeAlignment - stackIndex) / TR::Compiler->om.sizeofReferenceAddress()));

      if (comp()->getOption(TR_TraceRA))
         {
         traceMsg(comp(),"\nAlign stack offset before alignment = %d and after alignment = %d\n", stackIndexBeforeAlignment, stackIndex);
         }
      }
   }


////////////////////////////////////////////////////////////////////////////////
// J9::Z::PrivateLinkage::mapCompactedStack - maps variables onto the stack, sharing
//     stack slots for automatic variables with non-interfering live ranges.
////////////////////////////////////////////////////////////////////////////////
void
J9::Z::PrivateLinkage::mapCompactedStack(TR::ResolvedMethodSymbol * method)
   {
   ListIterator<TR::AutomaticSymbol>  automaticIterator(&method->getAutomaticList());
   TR::AutomaticSymbol               *localCursor       = automaticIterator.getFirst();
   int32_t                           firstLocalOffset  = getOffsetToFirstLocal();
   uint32_t                          stackIndex        = getOffsetToFirstLocal();
   TR::GCStackAtlas                  *atlas             = cg()->getStackAtlas();
   int32_t                           i;
   uint8_t                           pointerSize       = TR::Compiler->om.sizeofReferenceAddress();


#ifdef DEBUG
   uint32_t origSize = 0; // the size of the stack had we not compacted it
#endif

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   int32_t *colourToOffsetMap =
      (int32_t *) trMemory()->allocateStackMemory(cg()->getLocalsIG()->getNumberOfColoursUsedToColour() * sizeof(int32_t));

   uint32_t *colourToSizeMap =
      (uint32_t *) trMemory()->allocateStackMemory(cg()->getLocalsIG()->getNumberOfColoursUsedToColour() * sizeof(uint32_t));

   for (i=0; i<cg()->getLocalsIG()->getNumberOfColoursUsedToColour(); i++)
      {
      colourToOffsetMap[i] = -1;
      colourToSizeMap[i] = 0;
      }

   // Find maximum allocation size for each shared local.
   //
   TR_IGNode    *igNode;
   uint32_t      size;
   IGNodeColour  colour;

   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      {
      igNode = cg()->getLocalsIG()->getIGNodeForEntity(localCursor);
      if(igNode != NULL) // if the local doesn't have an interference graph node, we will just map it without attempt to compact, so we can ignore it
         {
         colour = igNode->getColour();

         TR_ASSERT(colour != UNCOLOURED, "uncoloured local %p (igNode=%p) found in locals IG\n",
                 localCursor, igNode);

         if (!(localCursor->isInternalPointer() || localCursor->isPinningArrayPointer() || localCursor->holdsMonitoredObject()))
            {
            size = localCursor->getRoundedSize();
            if (size > colourToSizeMap[colour])
               {
               colourToSizeMap[colour] = size;
               }
            }
         }
      }

   ListIterator<TR::AutomaticSymbol> variableSizeSymIterator(&method->getVariableSizeSymbolList());
   TR::AutomaticSymbol * variableSizeSymCursor = variableSizeSymIterator.getFirst();
   for (localCursor = variableSizeSymIterator.getFirst(); localCursor; localCursor = variableSizeSymIterator.getNext())
      {
      TR_ASSERT(localCursor->isVariableSizeSymbol(), "Should be variable sized");
      igNode = cg()->getLocalsIG()->getIGNodeForEntity(localCursor);
      if(igNode != NULL) // if the local doesn't have an interference graph node, we will just map it without attempt to compact, so we can ignore it
         {
         colour = igNode->getColour();
         TR_ASSERT(colour != UNCOLOURED, "uncoloured local %p (igNode=%p) found in locals IG\n",
                 localCursor, igNode);
         if (!(localCursor->isInternalPointer() || localCursor->isPinningArrayPointer() || localCursor->holdsMonitoredObject()))
            {
            size = localCursor->getRoundedSize();
            if (size > colourToSizeMap[colour])
               {
               colourToSizeMap[colour] = size;
               }
            }
         }
      }

   // *************************************how we align local objects********************************
   // because the offset of a local object is (stackIndex + pointerSize*(localCursor->getGCMapIndex()-firstLocalGCIndex))
   // In createStackAtlas, we align pointerSize*(localCursor->getGCMapIndex()-firstLocalGCIndex) by modifying local objects' gc indices
   // Here we align the stackIndex
   // *************************************how we align local objects********************************
   //
   traceMsg(comp(), "stackIndex after compaction = %d\n", stackIndex);

   // stackIndex in mapCompactedStack is calculated using only local reference sizes and does not include the padding
   stackIndex -= pointerSize * atlas->getNumberOfPaddingSlots();

   traceMsg(comp(), "stackIndex after padding slots = %d\n", stackIndex);

   uint32_t localObjectAlignment = 1 << TR::Compiler->om.compressedReferenceShift();

   if (localObjectAlignment >= 16)
      {
      // we don't want to fail gc when it tries to uncompress the reference of a stack allocated object, so we aligned the local objects based on the shift amount
      // this is different to the alignment of heap objects, which is controlled separately and could be larger than 2<<shiftamount
      alignLocalsOffset(stackIndex, localObjectAlignment);
      }

   // Map all garbage collected references together so we can concisely represent
   // stack maps. They must be mapped so that the GC map index in each local
   // symbol is honoured.
   //
#ifdef DEBUG
   // to report diagnostic information into the trace log that is guarded by if(debug("reportCL"))
   // set the environment variable TR_DEBUG=reportCL
   // also note that all diagnostic information is only reported in a debug build
   if(debug("reportCL"))
      diagnostic("\n****Mapping compacted stack for method: %s\n",comp()->signature());
#endif

   // Here we map the garbage collected references onto the stack
   // This stage is reversed later on, since in CodeGenGC we actually set all of the GC offsets
   // so effectively the local stack compaction of collected references happens there
   // but we must perform this stage to determine the size of the stack that contains object temp slots
   int32_t lowGCOffset = stackIndex;
   int32_t firstLocalGCIndex = atlas->getNumberOfParmSlotsMapped();
   automaticIterator.reset();
   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      {
      if (localCursor->getGCMapIndex() >= 0)
         {
         TR_IGNode *igNode;
         if (igNode = cg()->getLocalsIG()->getIGNodeForEntity(localCursor))
            {
            IGNodeColour colour = igNode->getColour();

            if (localCursor->isInternalPointer() || localCursor->isPinningArrayPointer() || localCursor->holdsMonitoredObject())
               {
               // Regardless of colouring on the local, map an internal
               // pointer or a pinning array local. These kinds of locals
               // do not participate in the compaction of locals phase and
               // are handled specially (basically the slots are not shared for
               // these autos).
               //
#ifdef DEBUG
               if(debug("reportCL"))
                  diagnostic("Mapping uncompactable ref local: %p\n",localCursor);
#endif
               mapSingleAutomatic(localCursor, stackIndex);
               }
            else if (colourToOffsetMap[colour] == -1)
               {
#ifdef DEBUG
               if(debug("reportCL"))
                  diagnostic("Mapping first ref local: %p (colour=%d)\n",localCursor, colour);
#endif
               mapSingleAutomatic(localCursor, stackIndex);
               colourToOffsetMap[colour] = localCursor->getOffset();
               }
            else
               {
                  traceMsg(comp(), "O^O COMPACT LOCALS: Sharing slot for local %p (colour = %d)\n",localCursor, colour);
                  localCursor->setOffset(colourToOffsetMap[colour]);
               }
            }
         else
            {
#ifdef DEBUG
            if(debug("reportCL"))
               diagnostic("No ig node exists for ref local %p, mapping regularly\n",localCursor);
#endif
            mapSingleAutomatic(localCursor, stackIndex);
            }

#ifdef DEBUG
         origSize += localCursor->getRoundedSize();
#endif
         }
      }

   // Here is where we reverse the previous stage
   // We map local references again to set the stack position correct according to
   // the GC map index, which is set in CodeGenGC
   //
   automaticIterator.reset();
   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      if (localCursor->getGCMapIndex() >= 0)
         {
         int32_t newOffset = stackIndex + pointerSize*(localCursor->getGCMapIndex()-firstLocalGCIndex);

         if (comp()->getOption(TR_TraceRA))
            traceMsg(comp(), "\nmapCompactedStack: changing %s (GC index %d) offset from %d to %d",
               comp()->getDebug()->getName(localCursor), localCursor->getGCMapIndex(), localCursor->getOffset(), newOffset);

         localCursor->setOffset(newOffset);

         TR_ASSERT((localCursor->getOffset() <= 0), "Local %p (GC index %d) offset cannot be positive (stackIndex = %d)\n", localCursor, localCursor->getGCMapIndex(), stackIndex);

         if (localCursor->getGCMapIndex() == atlas->getIndexOfFirstInternalPointer())
            {
            atlas->setOffsetOfFirstInternalPointer(localCursor->getOffset() - firstLocalOffset);
            }
         }

   method->setObjectTempSlots((lowGCOffset-stackIndex) / pointerSize);
   lowGCOffset = stackIndex;

   // Now map the rest of the locals (i.e. non-references)
   //
   // first map 4-byte locals, then 8-byte (and larger) locals
   //
   stackIndex -= (stackIndex & 0x4) ? 4 : 0;
   automaticIterator.reset();
   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      if (localCursor->getGCMapIndex() < 0)
         {
         TR_IGNode *igNode;
         if (igNode = cg()->getLocalsIG()->getIGNodeForEntity(localCursor))
            {
            IGNodeColour colour = igNode->getColour();

            if(colourToSizeMap[colour] < 8)
               {
               if (colourToOffsetMap[colour] == -1) // map auto to stack slot
                  {
#ifdef DEBUG
               if(debug("reportCL"))
                  diagnostic("Mapping first local: %p (colour=%d)\n",localCursor, colour);
#endif
                  mapSingleAutomatic(localCursor, colourToSizeMap[colour], stackIndex);
                  colourToOffsetMap[colour] = localCursor->getOffset();
                  }
               else // share local with already mapped stack slot
                  {
                     traceMsg(comp(), "O^O COMPACT LOCALS: Sharing slot for local %p (colour = %d)\n",localCursor, colour);
                     localCursor->setOffset(colourToOffsetMap[colour]);
                  }
#ifdef DEBUG
               origSize += localCursor->getRoundedSize();
#endif
               }
            }
         else if(localCursor->getRoundedSize() < 8)
            {
#ifdef DEBUG
            if(debug("reportCL"))
               diagnostic("No ig node exists for local %p, mapping regularly\n",localCursor);
            origSize += localCursor->getRoundedSize();
#endif
            mapSingleAutomatic(localCursor, stackIndex);
            }
         }


      variableSizeSymIterator.reset();
      variableSizeSymCursor = variableSizeSymIterator.getFirst();
      while (variableSizeSymCursor != NULL)
         {
         if (variableSizeSymCursor->isReferenced())
            {
              if (cg()->traceBCDCodeGen())
                 traceMsg(comp(),"map variableSize sym %p (size %d) because isReferenced=true ",variableSizeSymCursor,variableSizeSymCursor->getSize());
              mapSingleAutomatic(variableSizeSymCursor, stackIndex);  //Ivan
             if (cg()->traceBCDCodeGen())
                 traceMsg(comp(),"to auto offset %d\n",variableSizeSymCursor->getOffset());
            }
         else if (cg()->traceBCDCodeGen())
            {
            traceMsg(comp(),"do not map variableSize sym %p (size %d) because isReferenced=false\n",variableSizeSymCursor,variableSizeSymCursor->getSize());
            }
         variableSizeSymCursor = variableSizeSymIterator.getNext();
         }

   // Ensure the frame is double-word aligned, since we're about to map 8-byte autos
   //
#ifdef DEBUG
   origSize += (origSize & 0x4) ? 4 : 0;
#endif
   stackIndex -= (stackIndex & 0x4) ? 4 : 0;

   TR_ASSERT((stackIndex % pointerSize) == 0,
          "size of scalar temp area not a multiple of Java pointer size");

   // now map 8-byte autos
   //
   automaticIterator.reset();
   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      if (localCursor->getGCMapIndex() < 0)
         {
         TR_IGNode *igNode;
         if (igNode = cg()->getLocalsIG()->getIGNodeForEntity(localCursor))
            {
            IGNodeColour colour = igNode->getColour();

            if(colourToSizeMap[colour] >= 8)
               {
               if (colourToOffsetMap[colour] == -1) // map auto to stack slot
                  {
#ifdef DEBUG
               if(debug("reportCL"))
                  diagnostic("Mapping first local: %p (colour=%d)\n",localCursor, colour);
#endif
                  stackIndex -= (stackIndex & 0x4) ? 4 : 0;
                  mapSingleAutomatic(localCursor, colourToSizeMap[colour], stackIndex);
                  colourToOffsetMap[colour] = localCursor->getOffset();
                  }
               else // share local with already mapped stack slot
                  {
                     traceMsg(comp(), "O^O COMPACT LOCALS: Sharing slot for local %p (colour = %d)\n",localCursor, colour);
                     localCursor->setOffset(colourToOffsetMap[colour]);
                  }
#ifdef DEBUG
               origSize += localCursor->getRoundedSize();
#endif
               }
            }
         else if(localCursor->getRoundedSize() >= 8)
            {
#ifdef DEBUG
            if(debug("reportCL"))
               diagnostic("No ig node exists for local %p, mapping regularly\n",localCursor);
            origSize += localCursor->getRoundedSize();
#endif
            stackIndex -= (stackIndex & 0x4) ? 4 : 0;
            mapSingleAutomatic(localCursor, stackIndex);
            }
         }

   // Map slot for Long Displacement
   // Pick an arbitrary large number that is less than
   // long disp (4K) to identify that we are no-where near
   // a large stack or a large lit-pool

   //stackIndex -= pointerSize;
   stackIndex -= 16;   // see defect 162458, 164661
#ifdef DEBUG
   //  origSize += pointerSize;
   origSize += 16;
#endif
   setOffsetToLongDispSlot((uint32_t) (-((int32_t)stackIndex)));


   // msf - aligning the start of the parm list may not always
   // be best, but if a long is passed into a virtual fn, it will
   // then be aligned (and therefore can efficiently be accessed)
   // a better approach would be to look at the signature and determine
   // the best overall way to align the stack given that the parm list
   // is contiguous in storage to make it easy on the interpreter
   // and therefore there may not be a 'best' way to align the storage.
   // This change was made upon noticing that sometimes getObject() is
   // very hot and references its data from backing storage often.
   // it is possible that the stack might not be double-word aligned, due to mapping for long displacement if the pointer size is 4
#ifdef DEBUG
   origSize += (origSize & 0x4) ? 4 : 0;
#endif
   stackIndex -= (stackIndex & 0x4) ? 4 : 0;

   method->setScalarTempSlots((lowGCOffset-stackIndex) / pointerSize);
   method->setLocalMappingCursor(stackIndex);

   mapIncomingParms(method);

   atlas->setLocalBaseOffset(lowGCOffset - firstLocalOffset);
   atlas->setParmBaseOffset(atlas->getParmBaseOffset() + getOffsetToFirstParm() - firstLocalOffset);

   } // scope of the stack memory region

#ifdef DEBUG
   automaticIterator.reset();

   // report stack mapping even if TR_DEBUG=reportCL isn't set
   diagnostic("\n****SYMBOL OFFSETS\n");
   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      {
      diagnostic("Local %p, offset=%d\n", localCursor, localCursor->getOffset());
      }

   if (debug("reportCL"))
      {

      int mappedSize = firstLocalOffset - stackIndex;
      diagnostic("\n**** Mapped locals size: %d (orig map size=%d, shared size=%d)  %s\n",
                  (mappedSize),
                  origSize,
                  origSize - mappedSize,
                  comp()->signature());
      }
#endif

   }

   void
J9::Z::PrivateLinkage::mapStack(TR::ResolvedMethodSymbol * method)
   {

   if (cg()->getLocalsIG() && cg()->getSupportsCompactedLocals())
      {
      mapCompactedStack(method);
      return;
      }


   ListIterator<TR::AutomaticSymbol> automaticIterator(&method->getAutomaticList());
   TR::AutomaticSymbol * localCursor = automaticIterator.getFirst();
   TR::RealRegister::RegNum regIndex;
   int32_t firstLocalOffset = getOffsetToFirstLocal();
   uint32_t stackIndex = firstLocalOffset;
   int32_t lowGCOffset;
   TR::GCStackAtlas * atlas = cg()->getStackAtlas();

   // map all garbage collected references together so can concisely represent
   // stack maps. They must be mapped so that the GC map index in each local
   // symbol is honoured.
   lowGCOffset = stackIndex;
   int32_t firstLocalGCIndex = atlas->getNumberOfParmSlotsMapped();

   stackIndex -= (atlas->getNumberOfSlotsMapped() - firstLocalGCIndex) * TR::Compiler->om.sizeofReferenceAddress();

   uint32_t localObjectAlignment = 1 << TR::Compiler->om.compressedReferenceShift();

   if (localObjectAlignment >= 16)
      {
      // we don't want to fail gc when it tries to uncompress the reference of a stack allocated object, so we aligned the local objects based on the shift amount
      // this is different to the alignment of heap objects, which is controlled separately and could be larger than 2<<shiftamount
      alignLocalsOffset(stackIndex, localObjectAlignment);
      }

   // Map local references again to set the stack position correct according to
   // the GC map index.
   //
   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      {
      if (localCursor->getGCMapIndex() >= 0)
         {
         localCursor->setOffset(stackIndex + TR::Compiler->om.sizeofReferenceAddress() * (localCursor->getGCMapIndex() - firstLocalGCIndex));
         }
      if (localCursor->getGCMapIndex() == atlas->getIndexOfFirstInternalPointer())
         {
         atlas->setOffsetOfFirstInternalPointer(localCursor->getOffset() - firstLocalOffset);
         }
      }

   method->setObjectTempSlots((lowGCOffset - stackIndex) / TR::Compiler->om.sizeofReferenceAddress());
   lowGCOffset = stackIndex;

   stackIndex -= (stackIndex & 0x4) ? 4 : 0;

   // Now map the rest of the locals
   //
   ListIterator<TR::AutomaticSymbol> variableSizeSymIterator(&method->getVariableSizeSymbolList());
   TR::AutomaticSymbol * variableSizeSymCursor = variableSizeSymIterator.getFirst();
   while (variableSizeSymCursor != NULL)
      {
      TR_ASSERT(variableSizeSymCursor->isVariableSizeSymbol(), "should be variable sized");
      if (variableSizeSymCursor->isReferenced())
         {
         if (cg()->traceBCDCodeGen())
            traceMsg(comp(),"map variableSize sym %p (size %d) because isReferenced=true ",variableSizeSymCursor,variableSizeSymCursor->getSize());
         mapSingleAutomatic(variableSizeSymCursor, stackIndex);  //Ivan
         if (cg()->traceBCDCodeGen())
            traceMsg(comp(),"to auto offset %d\n",variableSizeSymCursor->getOffset());
         }
      else if (cg()->traceBCDCodeGen())
         {
         traceMsg(comp(),"do not map variableSize sym %p (size %d) because isReferenced=false\n",variableSizeSymCursor,variableSizeSymCursor->getSize());
         }
      variableSizeSymCursor = variableSizeSymIterator.getNext();
      }

   automaticIterator.reset();
   localCursor = automaticIterator.getFirst();

   while (localCursor != NULL)
      {
      if (localCursor->getGCMapIndex() < 0 && !TR::Linkage::needsAlignment(localCursor->getDataType(), cg()))
         {
         mapSingleAutomatic(localCursor, stackIndex);
         }
      localCursor = automaticIterator.getNext();
      }

   automaticIterator.reset();
   localCursor = automaticIterator.getFirst();

   // align double - but there is more to do to align the stack in general as double.
   while (localCursor != NULL)
      {
      if (localCursor->getGCMapIndex() < 0 && TR::Linkage::needsAlignment(localCursor->getDataType(), cg()))
         {
         stackIndex -= (stackIndex & 0x4) ? 4 : 0;
         mapSingleAutomatic(localCursor, stackIndex);
         }
      localCursor = automaticIterator.getNext();
      }

   // Force the stack size to be increased by...
   if (comp()->getOption(TR_Randomize) && comp()->getOptions()->get390StackBufferSize() == 0)
      {
      if (cg()->randomizer.randomBoolean(300) && performTransformation(comp(),"O^O Random Codegen  - Added 5000 dummy slots to Java Stack frame to test large displacement.\n"))
         {
         stackIndex -= 5000;
         }
      else
         {
         stackIndex -= 0;
         }
      }
   else
      {
      stackIndex -= (comp()->getOptions()->get390StackBufferSize()/4)*4;
      }


   stackIndex -= (stackIndex & 0x4) ? 4 : 0;

   // Pick an arbitrary large number that is less than
   // long disp (4K) to identify that we are no-where near
   // a large stack or a large lit-pool
   //

   stackIndex -= 16;   // see defect 162458, 164661
   setOffsetToLongDispSlot((uint32_t) (-((int32_t)stackIndex)));

   method->setScalarTempSlots((lowGCOffset - stackIndex) / TR::Compiler->om.sizeofReferenceAddress());
   method->setLocalMappingCursor(stackIndex);

   // msf - aligning the start of the parm list may not always
   // be best, but if a long is passed into a virtual fn, it will
   // then be aligned (and therefore can efficiently be accessed)
   // a better approach would be to look at the signature and determine
   // the best overall way to align the stack given that the parm list
   // is contiguous in storage to make it easy on the interpreter
   // and therefore there may not be a 'best' way to align the storage.
   // This change was made upon noticing that sometimes getObject() is
   // very hot and references it's data from backing storage often.
   stackIndex -= (stackIndex & 0x4) ? 4 : 0;

   mapIncomingParms(method);

   atlas->setLocalBaseOffset(lowGCOffset - firstLocalOffset);
   atlas->setParmBaseOffset(atlas->getParmBaseOffset() + getOffsetToFirstParm() - firstLocalOffset);

#ifdef DEBUG
      automaticIterator.reset();
      diagnostic("\n****SYMBOL OFFSETS\n");
      for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
         {
         diagnostic("Local %p, offset=%d\n", localCursor, localCursor->getOffset());
         }
#endif

   }

////////////////////////////////////////////////////////////////////////////////
// J9::Z::PrivateLinkage::mapSingleAutomatic - maps an automatic onto the stack
// with size p->getRoundedSize()
////////////////////////////////////////////////////////////////////////////////
void
J9::Z::PrivateLinkage::mapSingleAutomatic(TR::AutomaticSymbol * p, uint32_t & stackIndex)
   {

   mapSingleAutomatic(p, p->getRoundedSize(), stackIndex);
   }

////////////////////////////////////////////////////////////////////////////////
// J9::Z::PrivateLinkage::mapSingleAutomatic - maps an automatic onto the stack
////////////////////////////////////////////////////////////////////////////////
void
J9::Z::PrivateLinkage::mapSingleAutomatic(TR::AutomaticSymbol * p, uint32_t size, uint32_t & stackIndex)
   {

   p->setOffset(stackIndex -= size);
   }

bool
J9::Z::PrivateLinkage::hasToBeOnStack(TR::ParameterSymbol * parm)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   TR::ResolvedMethodSymbol * bodySymbol = comp()->getJittedMethodSymbol();
   TR_OpaqueClassBlock * throwableClass;

   // Need to save parameter on the stack if:
   //   A global register is allocated for the parameter AND either:
   //      1. the parameter is the *this pointer of a virtual sync'd/jvmpi method
   //      2. the address of the parameter is taken (JNI calls)
   //             (You can't get an address of the parameter if it is stored in a register -
   //              hence, parameter needs to be saved it onto the stack).
   bool result = (  parm->getAssignedGlobalRegisterIndex() >= 0 &&   // is using global RA
            (  (  parm->getLinkageRegisterIndex() == 0 &&            // is first parameter (this pointer)
                  parm->isCollectedReference() &&                    // is object reference
                  !bodySymbol->isStatic() &&                         // is virtual
// TODO:
// We potentially only need to save param onto stack for sync'd methods
// which have calls/exception traps.  Currently, we conservatively save
// param onto stack for sync'd methods, regardless of whether there are calls
// or not.
// see PPCLinkage for actual details.
//                (  (  bodySymbol->isSynchronised() &&                 // is sync method
//                      (  cg()->canExceptByTrap() || cg()->hasCall()  ) // can trigger stack walker
//                   ) ||
                  ( ( bodySymbol->isSynchronised()
                    ) ||
                    (
                    !strncmp(bodySymbol->getResolvedMethod()->nameChars(), "<init>", 6) &&
                      ( (throwableClass = fej9->getClassFromSignature("Ljava/lang/Throwable;", 21, bodySymbol->getResolvedMethod())) == 0 ||
                        fej9->isInstanceOf(bodySymbol->getResolvedMethod()->containingClass(), throwableClass, true) != TR_no
                      )
                    )
                  )
               ) ||
               parm->isParmHasToBeOnStack()                            // JNI direct where the address of a parm can be taken. e.g. &this.
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
   // a very hacky fix to a very specific problem. Also why is this code not commoned up with P and why
   // is it missing for X?

   if (result)
      parm->setParmHasToBeOnStack();

   return result;
   }

void
J9::Z::PrivateLinkage::setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol * method)
   {
   self()->setParameterLinkageRegisterIndex(method, method->getParameterList());
   }

void
J9::Z::PrivateLinkage::setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol * method, List<TR::ParameterSymbol> &parmList)
   {
   ListIterator<TR::ParameterSymbol> paramIterator(&parmList);
   TR::ParameterSymbol * paramCursor=paramIterator.getFirst();
   int32_t numIntArgs = 0, numFloatArgs = 0, numVectorArgs = 0;

   int32_t paramNum = -1;
   while ((paramCursor != NULL) &&
          (numIntArgs < self()->getNumIntegerArgumentRegisters() ||
           numFloatArgs < self()->getNumFloatArgumentRegisters() ||
           numVectorArgs < self()->getNumVectorArgumentRegisters()))
      {
      int32_t index = -1;
      paramNum++;

      TR::DataType dt = paramCursor->getDataType();

      switch (dt)
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address:
            if (numIntArgs < self()->getNumIntegerArgumentRegisters())
               {
               index = numIntArgs;
               }
            numIntArgs++;
            break;
         case TR::Int64:
            if(numIntArgs < self()->getNumIntegerArgumentRegisters())
               {
               index = numIntArgs;
               }
            numIntArgs += (comp()->target().is64Bit() ? 1 : 2);
            break;
         case TR::Float:
         case TR::Double:
            if (numFloatArgs < self()->getNumFloatArgumentRegisters())
               {
               index = numFloatArgs;
               }
            numFloatArgs++;
            break;
         case TR::PackedDecimal:
         case TR::ZonedDecimal:
         case TR::ZonedDecimalSignLeadingEmbedded:
         case TR::ZonedDecimalSignLeadingSeparate:
         case TR::ZonedDecimalSignTrailingSeparate:
         case TR::UnicodeDecimal:
         case TR::UnicodeDecimalSignLeading:
         case TR::UnicodeDecimalSignTrailing:
         case TR::Aggregate:
            break;
         default:
            if (dt.isVector())
               {
               // TODO: special handling for Float?
               if (numVectorArgs < self()->getNumVectorArgumentRegisters())
                  {
                  index = numVectorArgs;
                  }
               numVectorArgs++;
               break;
               }
         }
      paramCursor->setLinkageRegisterIndex(index);
      paramCursor = paramIterator.getNext();

      if (self()->isFastLinkLinkageType())
         {
         if ((numFloatArgs == 1) || (numIntArgs >= self()->getNumIntegerArgumentRegisters()))
            {
            // force fastlink ABI condition of only one float parameter for fastlink parameter and it must be within first slots
            numFloatArgs = self()->getNumFloatArgumentRegisters();   // no more float args possible now
            }
         }
      }
   }

//Clears numBytes bytes of storage from baseOffset(srcReg)
static TR::Instruction *
initStg(TR::CodeGenerator * codeGen, TR::Node * node, TR::RealRegister * tmpReg, TR::RealRegister * srcReg,TR::RealRegister * itersReg, int32_t baseOffset, int32_t numBytes,
   TR::Instruction * cursor)
   {
   int32_t numIters = (numBytes / 256);
   TR::RealRegister * baseReg = NULL;
   TR::RealRegister * indexReg = tmpReg;

   TR_ASSERT( numBytes >= 0, "number of bytes to clear must be positive");
   TR_ASSERT( baseOffset >= 0, "starting offset must be positive");

   if ((numBytes < 4096) && (numIters * 256 + baseOffset < 4096))
      {
      baseReg = srcReg;
      }
   else
      {
      baseReg = tmpReg;

      // If we don't set the proper flag when we use GPR14 as a temp register
      // here during prologue creation, we won't restore the return address
      // into GPR14 in epilogue
      tmpReg->setHasBeenAssignedInMethod(true);

      if (baseOffset>=MIN_IMMEDIATE_VAL && baseOffset<=MAX_IMMEDIATE_VAL)
         {
         cursor = generateRRInstruction(codeGen, TR::InstOpCode::getLoadRegOpCode(), node, baseReg, srcReg, cursor);
         cursor = generateRIInstruction(codeGen, TR::InstOpCode::getAddHalfWordImmOpCode(), node, baseReg, baseOffset, cursor);
         }
      else  // Large frame situation
         {
         cursor = generateS390ImmToRegister(codeGen, node, baseReg, (intptr_t)(baseOffset), cursor);
         cursor = generateRRInstruction(codeGen, TR::InstOpCode::getAddRegOpCode(), node, baseReg, srcReg, cursor);
         }
      baseOffset = 0;
      }

   MemClearConstLenMacroOp op(node, node, codeGen, numBytes);
   return op.generate(baseReg, baseReg, indexReg, itersReg, baseOffset, cursor);
   }

int32_t
J9::Z::PrivateLinkage::calculateRegisterSaveSize(TR::RealRegister::RegNum firstUsedReg,
                                                 TR::RealRegister::RegNum lastUsedReg,
                                                 int32_t &registerSaveDescription,
                                                 int32_t &numIntSaved, int32_t &numFloatSaved)
   {
   int32_t regSaveSize = 0;
   // set up registerSaveDescription which looks the following
   //
   // 00000000  offsetfrombp    0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
   //                          gpr15                         gpr0
   //
   // The bit is set to 1 if the register is saved.
   int32_t i;
   if (lastUsedReg != TR::RealRegister::NoReg)
      {
      for (i = firstUsedReg ; i <= lastUsedReg ; ++i)
         {
         registerSaveDescription |= 1 << (i - 1);
         numIntSaved++;
         }
      }

#if defined(ENABLE_PRESERVED_FPRS)
   for (i = TR::RealRegister::FPR8 ; i <= TR::RealRegister::FPR15 ; ++i)
      {
      if ((getRealRegister(i))->getHasBeenAssignedInMethod())
         {
         numFloatSaved++;
         }
      }
#endif

   // calculate stackFramesize
   regSaveSize += numIntSaved * cg()->machine()->getGPRSize() +
                          numFloatSaved * cg()->machine()->getFPRSize();


   int32_t firstLocalOffset = getOffsetToFirstLocal();
   int32_t localSize = -1 * (int32_t) (comp()->getJittedMethodSymbol()->getLocalMappingCursor());  // Auto+Spill size

   return regSaveSize;
   }

int32_t
J9::Z::PrivateLinkage::setupLiteralPoolRegister(TR::Snippet *firstSnippet)
   {
   // setup literal pool register if needed
   // on freeway:
   // LARL r6, i2  <- where i2 = (addr of lit. pool-current addr)/2
   //
   // on non freeway:
   // BRAS r6, 4
   // <lit. pool addr>
   // L    r6, 0(r6)

   if (!cg()->isLiteralPoolOnDemandOn() && firstSnippet != NULL)
      {
      // The immediate operand will be patched when the actual address of the literal pool is known
      if (cg()->anyLitPoolSnippets())
         {
         return getLitPoolRealRegister()->getRegisterNumber();
         }
      }

   return -1;
   }

////////////////////////////////////////////////////////////////////////////////
// TS_390PrivateLinkage::createPrologue() - create prolog for private linkage
////////////////////////////////////////////////////////////////////////////////
void
J9::Z::PrivateLinkage::createPrologue(TR::Instruction * cursor)
   {
   TR::RealRegister * spReg = getStackPointerRealRegister();
   TR::RealRegister * lpReg = getLitPoolRealRegister();
   TR::RealRegister * epReg = getEntryPointRealRegister();
   TR::Snippet * firstSnippet = NULL;
   TR::Node * firstNode = comp()->getStartTree()->getNode();
   int32_t size = 0, argSize = 0, regSaveSize = 0, numIntSaved = 0, numFloatSaved = 0;
   int32_t registerSaveDescription = 0;
   int32_t firstLocalOffset = getOffsetToFirstLocal();
   int32_t i;
   TR::ResolvedMethodSymbol * bodySymbol = comp()->getJittedMethodSymbol();
   int32_t localSize = -1 * (int32_t) (bodySymbol->getLocalMappingCursor());  // Auto+Spill size

   // look for registers that need to be saved
   // Look between R6-R11
   //
   TR::RealRegister::RegNum firstUsedReg = getFirstSavedRegister(TR::RealRegister::GPR6,
                                                              TR::RealRegister::GPR12);
   TR::RealRegister::RegNum lastUsedReg  = getLastSavedRegister(TR::RealRegister::GPR6,
                                                              TR::RealRegister::GPR12);

   // compute the register save area
   regSaveSize = calculateRegisterSaveSize(firstUsedReg, lastUsedReg,
                                           registerSaveDescription,
                                           numIntSaved, numFloatSaved);

   if (0 && comp()->target().is64Bit())
      {
      argSize = cg()->getLargestOutgoingArgSize() * 2 + getOffsetToFirstParm();
      }
   else
      {
      argSize = cg()->getLargestOutgoingArgSize() + getOffsetToFirstParm();
      }
   size = regSaveSize + localSize + argSize;

   // TODO: Rename this option to "disableStackAlignment" as we can align to more than doubleword now
   if (!comp()->getOption(TR_DisableDoubleWordStackAlignment))
      {
      traceMsg(comp(), "Before stack alignment Framesize = %d, localSize = %d\n", size, localSize);

      uint32_t stackFrameAlignment = std::max(1 << TR::Compiler->om.compressedReferenceShift(), 8);

      // Represents the smallest non-negative x such that (size + x) % stackFrameAlignment == 0
      int32_t distanceToAlignment = (stackFrameAlignment - (size % stackFrameAlignment)) % stackFrameAlignment;

      localSize += distanceToAlignment;

      // Recompute the size with the new (potentially) updated localSize
      size = regSaveSize + localSize + argSize;

      traceMsg(comp(), "After stack alignment Framesize = %d, localSize = %d\n", size, localSize);
      }

   // Check for large stack
   bool largeStack = (size<MIN_IMMEDIATE_VAL || size>MAX_IMMEDIATE_VAL);

   if (comp()->getOption(TR_TraceCG))
      {
      traceMsg(comp(), "\n regSaveSize = %d localSize = %d argSize = %d firstLocalOffset = %d \n",regSaveSize,localSize,argSize,firstLocalOffset);
      traceMsg(comp(), " Framesize = %d \n",size);
      }

   TR_ASSERT( ((int32_t) size % 4 == 0), "misaligned stack detected");

   setOffsetToRegSaveArea(argSize);

   registerSaveDescription |= (localSize + firstLocalOffset + regSaveSize) << 16;

   cg()->setRegisterSaveDescription(registerSaveDescription);

   cg()->setFrameSizeInBytes(size + firstLocalOffset);


   int32_t offsetToLongDisp = size - getOffsetToLongDispSlot();
   setOffsetToLongDispSlot(offsetToLongDisp);
   if (comp()->getOption(TR_TraceCG))
      {
      traceMsg(comp(), "\n\nOffsetToLongDispSlot = %d\n", offsetToLongDisp);
      }

   // Is GPR14 ever used?  If not, we can avoid
   //
   //   setRaContextSaveNeeded((getRealRegister(TR::RealRegister::GPR14))->getHasBeenAssignedInMethod());

   //  We assume frame size is less than 32k
   //TR_ASSERT(size<=MAX_IMMEDIATE_VAL,
   //   "J9::Z::PrivateLinkage::createPrologue -- Frame size (0x%x) greater than 0x7FFF\n",size);

   TR::MemoryReference * retAddrMemRef = NULL;

   // * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
   //
   //                    I M P O R T A N T!
   //
   // when recovering from a failed recompile, for sampling, any patching
   // must be
   // reversed.  The reversal code assumes that STY R14,-[4,8](r5) is
   // generated for trex, and a nop.  If this ever changes,
   // TR::Recompilation::methodCannotBeRecompiled must be updated.
   //
   // * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

   TR::RealRegister * tempReg = getRealRegister(TR::RealRegister::GPR0);

   setFirstPrologueInstruction(cursor);
   static bool prologTuning = (feGetEnv("TR_PrologTuning")!=NULL);

   if (prologTuning)
      {
      retAddrMemRef = generateS390MemoryReference(spReg, size - cg()->machine()->getGPRSize(), cg());
      }
   else
      {
      int32_t offset = cg()->machine()->getGPRSize() * -1;
      retAddrMemRef = generateS390MemoryReference(spReg, offset, cg());
      cursor = generateRXInstruction(cg(), TR::InstOpCode::getExtendedStoreOpCode(), firstNode, getRealRegister(getReturnAddressRegister()),
         retAddrMemRef, cursor);
      }

   // adjust java stack frame pointer
   if (largeStack)
      {
      cursor = generateS390ImmToRegister(cg(), firstNode, tempReg, (intptr_t)(size * -1), cursor);
      cursor = generateRRInstruction(cg(), TR::InstOpCode::getAddRegOpCode(), firstNode, spReg, tempReg, cursor);
      }
   else
      {
      // Adjust stack pointer with LA (reduce AGI delay)
      cursor = generateRXInstruction(cg(), TR::InstOpCode::LAY, firstNode, spReg, generateS390MemoryReference(spReg,(size) * -1, cg()),cursor);
      }

   if (!comp()->isDLT())
      {
      // Check stackoverflow /////////////////////////////////////
      //Load the stack limit in a temporary reg ( use R14, as it is killed later anyways )
      TR::RealRegister * stackLimitReg = getRealRegister(TR::RealRegister::GPR14);
      TR::RealRegister * mdReg = getMethodMetaDataRealRegister();
      TR::MemoryReference * stackLimitMR = generateS390MemoryReference(mdReg, cg()->getStackLimitOffset(), cg());

      // Compare stackLimit and currentStackPointer
      cursor = generateRXInstruction(cg(), TR::InstOpCode::getCmpLogicalOpCode(), firstNode, spReg, stackLimitMR, cursor);

      // Call stackOverflow helper, if stack limit is less than current Stack pointer. (Stack grows downwards)
      TR::LabelSymbol * stackOverflowSnippetLabel = generateLabelSymbol(cg());
      TR::LabelSymbol * reStartLabel = generateLabelSymbol(cg());

      //Call Stack overflow helper
      cursor = generateS390BranchInstruction(cg(), TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, firstNode, stackOverflowSnippetLabel, cursor);

      TR::SymbolReference * stackOverflowRef = comp()->getSymRefTab()->findOrCreateStackOverflowSymbolRef(comp()->getJittedMethodSymbol());

      TR::Snippet * snippet =
         new (trHeapMemory()) TR::S390StackCheckFailureSnippet(cg(), firstNode, reStartLabel, stackOverflowSnippetLabel, stackOverflowRef, size - cg()->machine()->getGPRSize());

      cg()->addSnippet(snippet);

      // The stack overflow helper returns back here
      cursor = generateS390LabelInstruction(cg(), TR::InstOpCode::label, firstNode, reStartLabel, cursor);
      }

      // End of stack overflow checking code ////////////////////////
   static bool bppoutline = (feGetEnv("TR_BPRP_Outline")!=NULL);

   if (bppoutline && cg()->_outlineCall._frequency != -1)
      {
      cursor =  new (cg()->trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::LARL, firstNode, epReg, (cg()->_outlineCall._callSymRef)->getSymbol(),(cg()->_outlineCall._callSymRef), cursor, cg());

      TR::MemoryReference * tempMR = generateS390MemoryReference(epReg, 0, cg());
      cursor = generateS390BranchPredictionPreloadInstruction(cg(), TR::InstOpCode::BPP, firstNode, cg()->_outlineCall._callLabel, (int8_t) 0xD, tempMR, cursor);
      }
   if (bppoutline && cg()->_outlineArrayCall._frequency != -1)
      {
      cursor =  new (cg()->trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::LARL, firstNode, epReg, (cg()->_outlineArrayCall._callSymRef)->getSymbol(),(cg()->_outlineArrayCall._callSymRef), cursor, cg());

      TR::MemoryReference * tempMR = generateS390MemoryReference(epReg, 0, cg());
      cursor = generateS390BranchPredictionPreloadInstruction(cg(), TR::InstOpCode::BPP, firstNode, cg()->_outlineArrayCall._callLabel, (int8_t) 0xD, tempMR, cursor);
      }

      if (cg()->getSupportsRuntimeInstrumentation())
         cursor = TR::TreeEvaluator::generateRuntimeInstrumentationOnOffSequence(cg(), TR::InstOpCode::RION, firstNode, cursor, true);


      // save registers that are used by this method
      int32_t disp = argSize;
      TR::MemoryReference * rsa ;

      // save GPRs
      if (lastUsedReg != TR::RealRegister::NoReg)
         {
         rsa = generateS390MemoryReference(spReg, disp, cg());

         if (firstUsedReg != lastUsedReg)
            {
            cursor = generateRSInstruction(cg(), TR::InstOpCode::getStoreMultipleOpCode(), firstNode, getRealRegister(firstUsedReg), getRealRegister(lastUsedReg), rsa, cursor);
            }
         else
            {
            cursor = generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), firstNode, getRealRegister(firstUsedReg), rsa, cursor);
            }
         }
      disp += numIntSaved * cg()->machine()->getGPRSize();

#if defined(ENABLE_PRESERVED_FPRS)
      //save FPRs
      for (i = TR::RealRegister::FPR8 ; i <= TR::RealRegister::FPR15 ; ++i)
         {
         if ((getRealRegister(i))->getHasBeenAssignedInMethod())
            {
            cursor = generateRXInstruction(cg(), TR::InstOpCode::STD, firstNode, getRealRegister(i), generateS390MemoryReference(spReg, disp, cg()),
                     cursor);
            disp += cg()->machine()->getFPRSize();
            }
         }
#endif

   if (prologTuning)
      {
      if ( size>=MAXLONGDISP )
         {
         cursor = generateS390ImmToRegister(cg(), firstNode, epReg, (intptr_t)(retAddrMemRef->getOffset()), cursor);
         retAddrMemRef->setOffset(0);
         retAddrMemRef->setDispAdjusted();
         retAddrMemRef->setIndexRegister(epReg);
         }
      // Save return address(R14) on stack
      cursor = generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), firstNode, getRealRegister(getReturnAddressRegister()), retAddrMemRef, cursor);
      }


   // initialize local objects
   TR::GCStackAtlas * atlas = cg()->getStackAtlas();
   if (atlas)
      {
      // for large copies, we can use the literal pool reg as a temp
      // (for >4096 clearing) when it is implemented

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
         int32_t offsetLcls = atlas->getLocalBaseOffset() + firstLocalOffset;
         TR::RealRegister * tmpReg = getReturnAddressRealRegister();
         TR::RealRegister * itersReg = getRealRegister(TR::RealRegister::GPR0);

         int32_t initbytes = cg()->machine()->getGPRSize() * numLocalsToBeInitialized;

         //printf("\ncollected reference: init %d bytes at offset %d\n", initbytes, size+offsetLcls);

         cursor = initStg(cg(), firstNode, tmpReg, spReg, itersReg, size + offsetLcls, initbytes, cursor);
         if (atlas->getInternalPointerMap())
            {
            int32_t offsetIntPtr = atlas->getOffsetOfFirstInternalPointer() + firstLocalOffset;

            // Total number of slots to be initialized is number of pinning arrays +
            // number of derived internal pointer stack slots
            //
            int32_t initbytes = (atlas->getNumberOfDistinctPinningArrays() +
               atlas->getInternalPointerMap()->getNumInternalPointers()) * cg()->machine()->getGPRSize();

            //printf("\ninternal pointer: init %d bytes at offset %d\n", initbytes, size+offsetIntPtr);

            cursor = initStg(cg(), firstNode, tmpReg, spReg, itersReg, size + offsetIntPtr, initbytes, cursor);
            }
         }
      }

   firstSnippet = cg()->getFirstSnippet();
   if (setupLiteralPoolRegister(firstSnippet) > 0)
      {
      cursor = new (trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::LARL, firstNode, lpReg, firstSnippet, cursor, cg());
      }

      ListIterator<TR::AutomaticSymbol> variableSizeSymIterator(&bodySymbol->getVariableSizeSymbolList());
      TR::AutomaticSymbol * variableSizeSymCursor = variableSizeSymIterator.getFirst();

      while (variableSizeSymCursor != NULL)
      {
      TR_ASSERT(variableSizeSymCursor->isVariableSizeSymbol(), "Should be variable sized");
      variableSizeSymCursor->setOffset(variableSizeSymCursor->getOffset() + size);
      variableSizeSymCursor = variableSizeSymIterator.getNext();
      }
   ListIterator<TR::AutomaticSymbol> automaticIterator(&bodySymbol->getAutomaticList());
   TR::AutomaticSymbol * localCursor = automaticIterator.getFirst();

   while (localCursor != NULL)
      {
      localCursor->setOffset(localCursor->getOffset() + size);
      localCursor = automaticIterator.getNext();
      }

   ListIterator<TR::ParameterSymbol> parameterIterator(&bodySymbol->getParameterList());
   TR::ParameterSymbol * parmCursor = parameterIterator.getFirst();
   while (parmCursor != NULL)
      {
      parmCursor->setParameterOffset(parmCursor->getParameterOffset() + size);
      parmCursor = parameterIterator.getNext();
      }

   // Save or move arguments according to the result of register assignment.
   cursor = (TR::Instruction *) saveArguments(cursor, false);

   static const bool prefetchStack = feGetEnv("TR_PrefetchStack") != NULL;
   if (cg()->isPrefetchNextStackCacheLine() && prefetchStack)
      {
      cursor = generateRXInstruction(cg(), TR::InstOpCode::PFD, firstNode, 2, generateS390MemoryReference(spReg, -256, cg()), cursor);
      }

   // Cold Eyecatcher is used for padding of endPC so that Return Address for exception snippets will never equal the endPC.
   // -> stackwalker assumes valid RA must be < endPC (not <= endPC).
   cg()->CreateEyeCatcher(firstNode);
   setLastPrologueInstruction(cursor);
   }


////////////////////////////////////////////////////////////////////////////////
// TS_390PrivateLinkage::createEpilog() - create epilog for private linkage
//
//  Here is the sample epilog that we are currently generated
//
//  10 c0 d0 00                    LM      GPR6, GPR15,  40(,GPR11)
//  47 00 b0 00                    BC      GPR14
////////////////////////////////////////////////////////////////////////////////
void
J9::Z::PrivateLinkage::createEpilogue(TR::Instruction * cursor)
   {
   TR::RealRegister * spReg = getRealRegister(getStackPointerRegister());
   TR::Node * currentNode = cursor->getNode();
   TR::Node * nextNode = cursor->getNext()->getNode();
   TR::ResolvedMethodSymbol * bodySymbol = comp()->getJittedMethodSymbol();
   uint32_t size = bodySymbol->getLocalMappingCursor();
   int32_t frameSize = cg()->getFrameSizeInBytes();
   int32_t i, offset = 0;
   TR::MemoryReference * rsa;
   TR::RealRegister::RegNum lastUsedReg, firstUsedReg;
   TR::RegisterDependencyConditions * dep;
   TR::RealRegister * tempReg = getRealRegister(TR::RealRegister::GPR0);
   TR::RealRegister * epReg = getRealRegister(getEntryPointRegister());
   int32_t blockNumber = -1;

   bool enableBranchPreload = cg()->supportsBranchPreload();

   dep = cursor->getNext()->getDependencyConditions();
   offset = getOffsetToRegSaveArea();

   // Do Return Address restore
   uint32_t adjustSize = frameSize - getOffsetToFirstLocal();

   static const char *disableRARestoreOpt = feGetEnv("TR_DisableRAOpt");

   // Any one of these conditions will force us to restore RA
   bool restoreRA = disableRARestoreOpt                                                                  ||
                    !(performTransformation(comp(), "O^O No need to restore RAREG in epilog\n")) ||
                    getRealRegister(getReturnAddressRegister())->getHasBeenAssignedInMethod()                       ||
                    cg()->canExceptByTrap()                                                      ||
                    cg()->getExitPointsInMethod()                                                ||
                    bodySymbol->isEHAware()                                                              ||
                    comp()->getOption(TR_FullSpeedDebug);  // CMVC 195232 - FSD can modify RA slot at a GC point.
   setRaContextRestoreNeeded(restoreRA);

   if (getRaContextRestoreNeeded())
      {
      cursor = generateRXInstruction(cg(), TR::InstOpCode::getExtendedLoadOpCode(), nextNode,
                                      getRealRegister(getReturnAddressRegister()),
                                      generateS390MemoryReference(spReg, frameSize, cg()), cursor);
      }
   else
      {
      if (comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "No RAREG context restore needed in Epilog\n");
      }

   if (enableBranchPreload && (cursor->getNext() == cg()->_hottestReturn._returnInstr))
      {
      if (cg()->_hottestReturn._frequency > 6 && cg()->_hottestReturn._insertBPPInEpilogue)
         {
         cg()->_hottestReturn._returnLabel = generateLabelSymbol(cg());
         TR::MemoryReference * tempMR = generateS390MemoryReference(getRealRegister(getReturnAddressRegister()), 0, cg());
         cursor = generateS390BranchPredictionPreloadInstruction(cg(), TR::InstOpCode::BPP, nextNode, cg()->_hottestReturn._returnLabel, (int8_t) 0x6, tempMR, cursor);
         cg()->_hottestReturn._insertBPPInEpilogue = false;
         }
      }

   // Restore GPRs
   firstUsedReg = getFirstRestoredRegister(TR::RealRegister::GPR6, TR::RealRegister::GPR12);
   lastUsedReg = getLastRestoredRegister(TR::RealRegister::GPR6, TR::RealRegister::GPR12);
   rsa = generateS390MemoryReference(spReg, offset, cg());

   if (lastUsedReg != TR::RealRegister::NoReg)
      {
      if (firstUsedReg != lastUsedReg)
         {
         cursor = restorePreservedRegs(firstUsedReg, lastUsedReg, blockNumber, cursor, nextNode, spReg, rsa, getStackPointerRegister());
         }
      else
         {
         cursor = generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), nextNode, getRealRegister(firstUsedReg), rsa, cursor);
         }
      offset += cg()->machine()->getGPRSize() * (lastUsedReg - firstUsedReg + 1);
      }

#if defined(ENABLE_PRESERVED_FPRS)
   //Load FPRs
   for (i = TR::RealRegister::FPR8 ; i <= TR::RealRegister::FPR15 ; ++i)
      {
      if ((getRealRegister(i))->getHasBeenAssignedInMethod())
         {
         cursor = generateRXInstruction(cg(), TR::InstOpCode::LD, currentNode, getRealRegister(i),
                  generateS390MemoryReference(spReg, offset, cg()), cursor);
         offset += cg()->machine()->getFPRSize();
         }
      }
#endif

   // Pop frame
   // use LA/LAY to add immediate through displacement
   if (adjustSize < MAXDISP)
      {
      cursor = generateRXInstruction(cg(), TR::InstOpCode::LA, nextNode, spReg, generateS390MemoryReference(spReg,adjustSize,cg()),cursor);
      }
   else if (adjustSize<MAXLONGDISP)
      {
      cursor = generateRXInstruction(cg(), TR::InstOpCode::LAY, nextNode, spReg, generateS390MemoryReference(spReg,adjustSize,cg()),cursor);
      }
   else
      {
      cursor = generateS390ImmToRegister(cg(), nextNode, tempReg, (intptr_t)(adjustSize), cursor);
      cursor = generateRRInstruction(cg(), TR::InstOpCode::getAddRegOpCode(), nextNode, spReg, tempReg, cursor);
      }

   // Add RIOFF on Epilogue before we leave the JIT
   if (cg()->getSupportsRuntimeInstrumentation())
      cursor = TR::TreeEvaluator::generateRuntimeInstrumentationOnOffSequence(cg(), TR::InstOpCode::RIOFF, currentNode, cursor, true);


   if (enableBranchPreload)
      {
      if (cursor->getNext() == cg()->_hottestReturn._returnInstr)
         {
         if (cg()->_hottestReturn._frequency > 6)
            {
            cursor = generateS390LabelInstruction(cg(), TR::InstOpCode::label, currentNode, cg()->_hottestReturn._returnLabel, cursor);
            }
         }
      }

   cursor = generateS390RegInstruction(cg(), TR::InstOpCode::BCR, currentNode, getRealRegister(getReturnAddressRegister()), cursor);
   ((TR::S390RegInstruction *)cursor)->setBranchCondition(TR::InstOpCode::COND_BCR);

   }

////////////////////////////////////////////////////////////////////////////////
// J9::Z::PrivateLinkage::buildVirtualDispatch - build virtual function call
////////////////////////////////////////////////////////////////////////////////
void
J9::Z::PrivateLinkage::buildVirtualDispatch(TR::Node * callNode, TR::RegisterDependencyConditions * dependencies,
   TR::Register * vftReg, uint32_t sizeOfArguments)
   {
   TR::RegisterDependencyGroup * Dgroup = dependencies->getPreConditions();
   TR::SymbolReference * methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol * methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   TR::LabelSymbol * vcallLabel = generateLabelSymbol(cg());
   TR::Instruction * gcPoint = NULL;
   TR::Snippet *unresolvedSnippet = NULL;
   TR_Debug * debugObj = cg()->getDebug();

   TR_ResolvedMethod *  profiledMethod    = NULL;
   TR_OpaqueClassBlock *profiledClass     = NULL;
   bool                 useProfiledValues = false;

   if (comp()->getOption(TR_TraceCG))
      traceMsg(comp(), "Build Virtual Dispatch\n");

   if ((methodSymbol && !methodSymbol->isComputed()) &&
       (comp()->getPersistentInfo()->isRuntimeInstrumentationEnabled()) &&
       (comp()->getOption(TR_EnableRIEMIT)))
      {
      TR::Instruction *emitInstruction = generateRIInstruction(cg(), TR::InstOpCode::RIEMIT, callNode, vftReg, 0);
      comp()->addHWPValueProfileInstruction(emitInstruction);
      }

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());

   // Generate and register a thunk for a resolved virtual function
   void *virtualThunk;
   if (methodSymbol && methodSymbol->isComputed())
      {
      switch (methodSymbol->getMandatoryRecognizedMethod())
         {
         case TR::java_lang_invoke_ComputedCalls_dispatchVirtual:
         case TR::com_ibm_jit_JITHelpers_dispatchVirtual:
            {
            char *j2iSignature = fej9->getJ2IThunkSignatureForDispatchVirtual(methodSymbol->getMethod()->signatureChars(), methodSymbol->getMethod()->signatureLength(), comp());
            int32_t signatureLen = strlen(j2iSignature);
            virtualThunk = fej9->getJ2IThunk(j2iSignature, signatureLen, comp());
            if (!virtualThunk)
               {
               virtualThunk = fej9->setJ2IThunk(j2iSignature, signatureLen,
                  TR::S390J9CallSnippet::generateVIThunk(
                     fej9->getEquivalentVirtualCallNodeForDispatchVirtual(callNode, comp()), sizeOfArguments, cg()), comp()); // TODO:JSR292: Is this the right sizeOfArguments?
               }
            }
            break;
         default:
            if (fej9->needsInvokeExactJ2IThunk(callNode, comp()))
               {
               TR_MHJ2IThunk *thunk = TR::S390J9CallSnippet::generateInvokeExactJ2IThunk(callNode, sizeOfArguments, methodSymbol->getMethod()->signatureChars(), cg());
               fej9->setInvokeExactJ2IThunk(thunk, comp());
               }
            break;
         }
      }
   else
      {
      virtualThunk = fej9->getJ2IThunk(methodSymbol->getMethod(), comp());
      if (!virtualThunk)
         virtualThunk = fej9->setJ2IThunk(methodSymbol->getMethod(), TR::S390J9CallSnippet::generateVIThunk(callNode, sizeOfArguments, cg()), comp());
      }

   if (methodSymbol->isVirtual() && (!methodSymRef->isUnresolved() && !comp()->compileRelocatableCode()))
      {
      TR_ResolvedMethod * rsm = methodSymbol->castToResolvedMethodSymbol()->getResolvedMethod();

      // Simple heuristic to determine when to prefetch the next cache line in method prologue.
      // We check the J9ROMMethod of the non-cold callsite to estimate how big of a stack
      // frame will be required for the call.
      if (!(cg()->getCurrentEvaluationTreeTop()->getEnclosingBlock()->isCold()) &&
          (rsm->numberOfParameterSlots() + rsm->numberOfTemps()) > 5)
         {
         cg()->setPrefetchNextStackCacheLine(true);
         }
      }

   if (cg()->getSupportsRuntimeInstrumentation())
      TR::TreeEvaluator::generateRuntimeInstrumentationOnOffSequence(cg(), TR::InstOpCode::RIOFF, callNode);

   if (methodSymbol->isVirtual())
      {
      TR::Instruction * cursor = NULL;
      bool performGuardedDevirtualization = false;
      TR::LabelSymbol * virtualLabel     = NULL;
      TR::LabelSymbol * doneVirtualLabel = generateLabelSymbol(cg());
      int32_t offset = comp()->compileRelocatableCode() ? 0: methodSymRef->getOffset();

      if (comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "Virtual call with offset %d\n", offset);

      // We split dependencies to make sure the RA doesn't insert any register motion code in the fixed
      // block sequence.
      //
      TR::RegisterDependencyConditions * preDeps = new (trHeapMemory())
                          TR::RegisterDependencyConditions(dependencies->getPreConditions(), NULL,
                          dependencies->getAddCursorForPre(), 0, cg());

      //  Add the ThisReg to the postDeps to avoid seeing a SPILL inserted between the resolution code
      //  and the VTABLE.  This sequence is assumed to be fixed length.
      //  Added one more slot for the post dep that might be added in buildDirectCall
      //
      TR::RegisterDependencyConditions * postDepsTemp = new (trHeapMemory())
                          TR::RegisterDependencyConditions(NULL, dependencies->getPostConditions(), 0,
                          dependencies->getAddCursorForPost(), cg());
      TR::RegisterDependencyConditions * postDeps = new (trHeapMemory())
                          TR::RegisterDependencyConditions(postDepsTemp,0,4, cg());

      // Search ARG Deps for vregs used for RA/EP and this
      //
      TR::Register * RegZero   = dependencies->searchPostConditionRegister(TR::RealRegister::GPR0);
      TR::Register * RegThis = dependencies->searchPreConditionRegister(TR::RealRegister::GPR1);
      TR::Register * RegRA   = dependencies->searchPostConditionRegister(getReturnAddressRegister());

      // Check the thisChild to see if anyone uses this object after the call (if not, we won't add it to post Deps)
      if (callNode->getChild(callNode->getFirstArgumentIndex())->getReferenceCount() > 0)
         {
         postDeps->addPostCondition(RegThis, TR::RealRegister::AssignAny);
         }

      if (methodSymRef->isUnresolved() || comp()->compileRelocatableCode())
         {
         if (comp()->getOption(TR_TraceCG))
            traceMsg(comp(), "... virtual call is unresolved\n");

         // TODO: Task 124512. Fix picbuilder register preservation before
         // moving this vft register dependency to BASR pre-deps.
         postDeps->addPostConditionIfNotAlreadyInserted(vftReg, TR::RealRegister::AssignAny);

         // Emit the resolve snippet and BRASL to call it
         //
         TR::LabelSymbol * snippetLabel = generateLabelSymbol(cg());
         unresolvedSnippet = new (trHeapMemory()) TR::S390VirtualUnresolvedSnippet(cg(), callNode, snippetLabel, sizeOfArguments, virtualThunk);
         cg()->addSnippet(unresolvedSnippet);
         //generateSnippetCall extracts preDeps from dependencies and puts them on BRASL
         TR::Instruction * gcPoint =
            generateSnippetCall(cg(), callNode, unresolvedSnippet, dependencies, methodSymRef);
         gcPoint->setNeedsGCMap(getPreservedRegisterMapForGC());
         }
      else
         {
         if (comp()->getOption(TR_TraceCG))
            traceMsg(comp(), "...call resolved\n");

         TR::ResolvedMethodSymbol * resolvedSymbol = methodSymRef->getSymbol()->getResolvedMethodSymbol();
         TR_ResolvedMethod * resolvedMethod = resolvedSymbol ? resolvedSymbol->getResolvedMethod() : 0;

         if ((comp()->performVirtualGuardNOPing() && comp()->isVirtualGuardNOPingRequired()))
            {
            TR_VirtualGuard * virtualGuard;

            if (resolvedMethod &&
               !resolvedMethod->isInterpreted() &&
               !callNode->isTheVirtualCallNodeForAGuardedInlinedCall())
               {
               if (!resolvedMethod->virtualMethodIsOverridden() && !resolvedMethod->isAbstract())
                  {

                  performGuardedDevirtualization = true;

                  // Build guarded devirtualization dispatch.
                  //
                  virtualGuard = TR_VirtualGuard::createGuardedDevirtualizationGuard(TR_NonoverriddenGuard,
                                                      comp(), callNode);
                  if (comp()->getOption(TR_TraceCG))
                     {
                     traceMsg(comp(), "Emit new Non-Overridden guard for call %s (%x) in %s\n", resolvedMethod->signature(trMemory()), callNode,
                           comp()->signature());
                     }
                  }
               else
                  {
                  TR_OpaqueClassBlock * thisClass = resolvedMethod->containingClass();
                  TR_DevirtualizedCallInfo * devirtualizedCallInfo = comp()->findDevirtualizedCall(callNode);
                  TR_OpaqueClassBlock * refinedThisClass = 0;

                  if (devirtualizedCallInfo)
                     {
                     refinedThisClass = devirtualizedCallInfo->_thisType;
                     if (comp()->getOption(TR_TraceCG))
                        {
                        traceMsg(comp(), "Found refined this class info %x for call %x in %s\n", refinedThisClass, callNode,
                              comp()->signature());
                        }
                     if (refinedThisClass)
                        {
                        thisClass = refinedThisClass;
                        }
                     }

                  TR_PersistentCHTable * chTable = comp()->getPersistentInfo()->getPersistentCHTable();
                  /* Devirtualization is not currently supported for AOT compilations */
                  if (thisClass && TR::Compiler->cls.isAbstractClass(comp(), thisClass) && !comp()->compileRelocatableCode())
                     {
                     TR_ResolvedMethod * method = chTable->findSingleAbstractImplementer(thisClass, methodSymRef->getOffset(),
                                                                methodSymRef->getOwningMethod(comp()), comp());
                     if (method &&
                        (comp()->isRecursiveMethodTarget(method) || !method->isInterpreted() || method->isJITInternalNative()))
                        {
                        performGuardedDevirtualization = true;
                        resolvedMethod = method;
                        virtualGuard = TR_VirtualGuard::createGuardedDevirtualizationGuard(TR_AbstractGuard,
                                                            comp(), callNode);
                        if (comp()->getOption(TR_TraceCG))
                           {
                           traceMsg(comp(), "Emit new ABSTRACT guard for call %s (%x) in %s\n", resolvedMethod->signature(trMemory()), callNode,
                                 comp()->signature());
                           }
                        }
                     }
                  else if (refinedThisClass && !chTable->isOverriddenInThisHierarchy(resolvedMethod, refinedThisClass,
                                                            methodSymRef->getOffset(), comp()))
                     {
                     if (resolvedMethod->virtualMethodIsOverridden())
                        {
                        TR_ResolvedMethod * calleeMethod = methodSymRef->getOwningMethod(comp())->getResolvedVirtualMethod(comp(),
                                                                refinedThisClass, methodSymRef->getOffset());
                        if (calleeMethod &&
                           (comp()->isRecursiveMethodTarget(calleeMethod) ||
                           !calleeMethod->isInterpreted() ||
                           calleeMethod->isJITInternalNative()))
                           {
                           performGuardedDevirtualization = true;
                           resolvedMethod = calleeMethod;
                           virtualGuard = TR_VirtualGuard::createGuardedDevirtualizationGuard(TR_HierarchyGuard,
                                                               comp(), callNode);

                           if (comp()->getOption(TR_TraceCG))
                              {
                              traceMsg(comp(), "Emit new HierarchyGuardguard for call %s (%x) in %s\n", resolvedMethod->signature(trMemory()), callNode,
                                 comp()->signature());
                              }
                           }
                        }
                     }
                  }
               if (performGuardedDevirtualization && virtualGuard)
                  {
                  virtualLabel = vcallLabel;
                  generateVirtualGuardNOPInstruction(cg(), callNode, virtualGuard->addNOPSite(), NULL, virtualLabel);
                  if (comp()->getOption(TR_EnableHCR))
                     {
                     if (cg()->supportsMergingGuards())
                        {
                        virtualGuard->setMergedWithHCRGuard();
                        }
                     else
                        {
                        TR_VirtualGuard* HCRGuard = TR_VirtualGuard::createGuardedDevirtualizationGuard(TR_HCRGuard, comp(), callNode);
                        generateVirtualGuardNOPInstruction(cg(), callNode, HCRGuard->addNOPSite(), NULL, virtualLabel);
                        }
                     }
                  }
               }
            }

         if (!performGuardedDevirtualization &&
             !comp()->getOption(TR_DisableInterpreterProfiling) &&
             (callNode->getSymbolReference() != comp()->getSymRefTab()->findObjectNewInstanceImplSymbol()) &&
             TR_ValueProfileInfoManager::get(comp()) && resolvedMethod
             )
            {
            TR_AddressInfo *valueInfo = NULL;
            if (!comp()->compileRelocatableCode())
               valueInfo = static_cast<TR_AddressInfo*>(TR_ValueProfileInfoManager::getProfiledValueInfo(callNode, comp(), AddressInfo));

            uintptr_t topValue = valueInfo ? valueInfo->getTopValue() : 0;

            // Is the topValue valid?
            if( topValue )
               {
               if( valueInfo->getTopProbability() < MIN_PROFILED_CALL_FREQUENCY  ||
                   comp()->getPersistentInfo()->isObsoleteClass((void*)topValue, fej9) )
                  {
                  topValue = 0;
                  }
               else
                  {
                  TR_OpaqueClassBlock *callSiteMethodClass = methodSymRef->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod()->classOfMethod();
                  if (!cg()->isProfiledClassAndCallSiteCompatible((TR_OpaqueClassBlock *)topValue, callSiteMethodClass))
                     {
                     topValue = 0;
                     }
                  }
               }

            if ( topValue )
               {
               TR_ResolvedMethod *profiledVirtualMethod = methodSymRef->getOwningMethod(comp())->getResolvedVirtualMethod(comp(),
                          (TR_OpaqueClassBlock *)topValue, methodSymRef->getOffset());
               if (profiledVirtualMethod && !profiledVirtualMethod->isInterpreted())
                  {
                  if (comp()->getOption(TR_TraceCG))
                     {
                     traceMsg(comp(),
                           "Profiled method {%s}\n",
                           fej9->sampleSignature((TR_OpaqueMethodBlock *)(profiledVirtualMethod->getPersistentIdentifier()), 0, 0, comp()->trMemory()));
                     }
                  profiledMethod    = profiledVirtualMethod;
                  profiledClass     = (TR_OpaqueClassBlock *)topValue;
                  useProfiledValues = true;
                  virtualLabel = vcallLabel;
                  }
               }
            }

         if (performGuardedDevirtualization || useProfiledValues)
            {
            if (comp()->getOption(TR_TraceCG))
               traceMsg(comp(), "Make direct call under devirtualization\n");

            TR::SymbolReference * realMethodSymRef = methodSymRef;
            if (useProfiledValues || resolvedMethod != resolvedSymbol->getResolvedMethod())
               {
               realMethodSymRef= comp()->getSymRefTab()->findOrCreateMethodSymbol(methodSymRef->getOwningMethodIndex(),
                  -1, (useProfiledValues)?profiledMethod:resolvedMethod, TR::MethodSymbol::Virtual);
               }

            if (useProfiledValues)
               {
               genLoadProfiledClassAddressConstant(cg(), callNode, profiledClass, RegZero, NULL, dependencies, NULL);
               generateS390CompareAndBranchInstruction(cg(), TR::InstOpCode::getCmpLogicalRegOpCode(), callNode, vftReg, RegZero, TR::InstOpCode::COND_BNE, virtualLabel);
               }

            buildDirectCall(callNode, realMethodSymRef,  dependencies, sizeOfArguments);

            if (!virtualLabel)
               generateS390BranchInstruction(cg(), TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, callNode, doneVirtualLabel);
            }
         }

      TR_S390OutOfLineCodeSection *outlinedSlowPath = NULL;

      if ( virtualLabel )
         {
         traceMsg (comp(), "OOL vcall: generating Vcall dispatch sequence\n");
         //Using OOL but generating code manually
         outlinedSlowPath = new (cg()->trHeapMemory()) TR_S390OutOfLineCodeSection(vcallLabel,doneVirtualLabel,cg());
         cg()->getS390OutOfLineCodeSectionList().push_front(outlinedSlowPath);
         outlinedSlowPath->swapInstructionListsWithCompilation();

         TR::Instruction * temp = generateS390LabelInstruction(cg(), TR::InstOpCode::label, callNode, vcallLabel);
         if (debugObj)
            {
            debugObj->addInstructionComment(temp, "Denotes start of OOL vcall sequence");
            }
         }

      // load class pointer
      TR::Register *classReg = vftReg;

      // It should be impossible to have a offset that can't fit in 20bit given Java method table limitations.
      // We assert here to insure limitation/assumption remains true. If this fires we need to fix this code
      // and the _virtualUnresolvedHelper() code to deal with a new worst case scenario for patching.
      TR_ASSERT_FATAL(offset>MINLONGDISP, "JIT VFT offset does not fit in 20bits");
      TR_ASSERT_FATAL(offset!=0 || unresolvedSnippet, "Offset is 0 yet unresolvedSnippet is NULL");
      TR_ASSERT_FATAL(offset<=MAX_IMMEDIATE_VAL, "Offset is larger then MAX_IMMEDIATE_VAL");

      // If unresolved/AOT, this instruction will be patched by _virtualUnresolvedHelper() with the correct offset
      cursor = generateRXInstruction(cg(), TR::InstOpCode::getExtendedLoadOpCode(), callNode, RegRA,
                                       generateS390MemoryReference(classReg, offset, cg()));

      if (unresolvedSnippet)
         {
         ((TR::S390VirtualUnresolvedSnippet *)unresolvedSnippet)->setPatchVftInstruction(cursor);
         }

      // A load immediate into R0 instruction (LHI/LGFI) MUST be generated here because the "LA" instruction used by
      // the VM to find VFT table entries can't handle negative displacements. For unresolved/AOT targets we must assume
      // the worse case (offset can't fit in 16bits). VFT offset 0 means unresolved/AOT, otherwise offset is negative.
      // Some special cases have positive offsets i.e. java/lang/Object.newInstancePrototype()
      if (!unresolvedSnippet && offset >= MIN_IMMEDIATE_VAL && offset <= MAX_IMMEDIATE_VAL) // Offset fits in 16bits
         {
         cursor = generateRIInstruction(cg(), TR::InstOpCode::getLoadHalfWordImmOpCode(), callNode, RegZero, offset);
         }
      else // if unresolved || offset can't fit in 16bits
         {
         // If unresolved/AOT, this instruction will be patched by _virtualUnresolvedHelper() with the correct offset
         cursor = generateRILInstruction(cg(), TR::InstOpCode::LGFI, callNode, RegZero, static_cast<int32_t>(offset));
         }

      gcPoint = new (trHeapMemory()) TR::S390RRInstruction(TR::InstOpCode::BASR, callNode, RegRA, RegRA, cg());
      gcPoint->setDependencyConditions(preDeps);

      if (unresolvedSnippet != NULL)
         (static_cast<TR::S390VirtualUnresolvedSnippet *>(unresolvedSnippet))->setIndirectCallInstruction(gcPoint);

      if (outlinedSlowPath)
         {
         TR::Instruction * temp = generateS390BranchInstruction(cg(),TR::InstOpCode::BRC,TR::InstOpCode::COND_BRC,callNode,doneVirtualLabel);
         if (debugObj)
            {
            debugObj->addInstructionComment(temp, "Denotes end of OOL vcall sequence: return to mainline");
            }
         // Done using OOL with manual code generation
         outlinedSlowPath->swapInstructionListsWithCompilation();

         generateS390LabelInstruction(cg(), TR::InstOpCode::label, callNode, doneVirtualLabel, postDeps);
         }
      else
         {
         gcPoint->setDependencyConditions(postDeps);
         }
      }
   else if (methodSymbol->isInterface())
      {
      int32_t i=0;
      TR::Register * thisClassRegister;
      TR::Register * methodRegister ;
      TR::RegisterPair * classMethodEPPairRegister;
      int32_t numInterfaceCallCacheSlots =  comp()->getOptions()->getNumInterfaceCallCacheSlots();

      if (comp()->getOption(TR_disableInterfaceCallCaching))
         {
         numInterfaceCallCacheSlots=0;
         }
      else if (comp()->getOption(TR_enableInterfaceCallCachingSingleDynamicSlot))
         {
         numInterfaceCallCacheSlots=1;
         }

      TR_ValueProfileInfoManager *valueProfileInfo = TR_ValueProfileInfoManager::get(comp());
      TR_AddressInfo *info = NULL;
      uint32_t numStaticPICs = 0;
      if (valueProfileInfo)
         info = static_cast<TR_AddressInfo*>(valueProfileInfo->getValueInfo(callNode->getByteCodeInfo(), comp(), AddressInfo));

      TR::list<TR_OpaqueClassBlock*> * profiledClassesList = NULL;

      bool isAddressInfo = info != NULL;
        uint32_t totalFreq = info ? info->getTotalFrequency() : 0;
        bool isAOT = cg()->needClassAndMethodPointerRelocations();
        bool callIsSafe = methodSymRef != comp()->getSymRefTab()->findObjectNewInstanceImplSymbol();
        if (!isAOT && callIsSafe && isAddressInfo &&
              (totalFreq!=0 && info->getTopProbability() > MIN_PROFILED_CALL_FREQUENCY))
           {

           TR_ScratchList<TR_ExtraAddressInfo> allValues(comp()->trMemory());
           info->getSortedList(comp(), &allValues);

           TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
           TR_ResolvedMethod *owningMethod = methodSymRef->getOwningMethod(comp());

           ListIterator<TR_ExtraAddressInfo> valuesIt(&allValues);

           uint32_t maxStaticPICs = comp()->getOptions()->getNumInterfaceCallStaticSlots();

           TR_ExtraAddressInfo *profiledInfo;
           profiledClassesList = new (trHeapMemory()) TR::list<TR_OpaqueClassBlock*>(getTypedAllocator<TR_OpaqueClassBlock*>(comp()->allocator()));
           for (profiledInfo = valuesIt.getFirst();  numStaticPICs < maxStaticPICs && profiledInfo != NULL; profiledInfo = valuesIt.getNext())
              {

              float freq = (float) profiledInfo->_frequency / totalFreq;
              if (freq < MIN_PROFILED_CALL_FREQUENCY)
                 continue;

              TR_OpaqueClassBlock *clazz = (TR_OpaqueClassBlock *)profiledInfo->_value;
              if (comp()->getPersistentInfo()->isObsoleteClass(clazz, fej9))
                 continue;

              TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
              TR_ResolvedMethod * profiledMethod = methodSymRef->getOwningMethod(comp())->getResolvedInterfaceMethod(comp(),
                    (TR_OpaqueClassBlock *)clazz, methodSymRef->getCPIndex());

              if (profiledMethod && !profiledMethod->isInterpreted())
                 {
                 numInterfaceCallCacheSlots++;
                 numStaticPICs++;
                 profiledClassesList->push_front(clazz);
                 }
              }
        }

        if (comp()->getOption(TR_TraceCG))
           {
           if (numStaticPICs != 0)
              traceMsg(comp(), "Interface dispatch with %d cache slots, added extra %d slot(s) for profiled classes.\n", numInterfaceCallCacheSlots, numStaticPICs);
           else
              traceMsg(comp(), "Interface dispatch with %d cache slots\n", numInterfaceCallCacheSlots);
           }

      TR::LabelSymbol * snippetLabel = generateLabelSymbol(cg());
      TR::S390InterfaceCallSnippet * ifcSnippet = new (trHeapMemory()) TR::S390InterfaceCallSnippet(cg(), callNode,
           snippetLabel, sizeOfArguments, numInterfaceCallCacheSlots, virtualThunk, false);
      cg()->addSnippet(ifcSnippet);

      if (numStaticPICs != 0)
         cg()->addPICsListForInterfaceSnippet(ifcSnippet->getDataConstantSnippet(), profiledClassesList);

      if (numInterfaceCallCacheSlots == 0 )
         {
         //Disabled interface call caching
         TR::LabelSymbol * hitLabel = generateLabelSymbol(cg());
         TR::LabelSymbol * snippetLabel = generateLabelSymbol(cg());

         // Make a copy of input deps, but add on 3 new slots.
         TR::RegisterDependencyConditions * postDeps = new (trHeapMemory()) TR::RegisterDependencyConditions(dependencies, 0, 3, cg());
         postDeps->setAddCursorForPre(0);        // Ignore all pre-deps that were copied.
         postDeps->setNumPreConditions(0, trMemory());        // Ignore all pre-deps that were copied.

         gcPoint = generateSnippetCall(cg(), callNode, ifcSnippet, dependencies,methodSymRef);

         // NOP is necessary so that the VM doesn't confuse Virtual Dispatch (expected to always use BASR
         // with interface dispatch (which must guarantee that RA-2 != 0x0D ie. BASR)
         //
         TR::Instruction * cursor = new (trHeapMemory()) TR::S390NOPInstruction(TR::InstOpCode::NOP, 2, callNode, cg());

         // Fool the snippet into setting up the return address to be after the NOP
         //
         gcPoint = cursor;
         ((TR::S390CallSnippet *) ifcSnippet)->setBranchInstruction(gcPoint);
         cursor->setDependencyConditions(postDeps);
         }
      else
         {
         TR::Instruction * cursor = NULL;
         TR::LabelSymbol * paramSetupDummyLabel = generateLabelSymbol(cg());
         TR::LabelSymbol * returnLocationLabel = generateLabelSymbol(cg());
         TR::LabelSymbol * cacheFailLabel = generateLabelSymbol(cg());

         TR::Register * RegEP = dependencies->searchPostConditionRegister(getEntryPointRegister());
         TR::Register * RegRA = dependencies->searchPostConditionRegister(getReturnAddressRegister());
         TR::Register * RegThis = dependencies->searchPreConditionRegister(TR::RealRegister::GPR1);
         TR::Register * snippetReg = RegEP;


         // We split dependencies to make sure the RA doesn't insert any register motion code in the fixed
         // block sequence and to only enforce parameter setup on head of block.
         TR::RegisterDependencyConditions * preDeps = new (trHeapMemory()) TR::RegisterDependencyConditions(
            dependencies->getPreConditions(), NULL, dependencies->getAddCursorForPre(), 0, cg());

         // Make a copy of input deps, but add on 3 new slots.
         TR::RegisterDependencyConditions * postDeps = new (trHeapMemory()) TR::RegisterDependencyConditions(dependencies, 0, 5, cg());
         postDeps->setAddCursorForPre(0);        // Ignore all pre-deps that were copied.
         postDeps->setNumPreConditions(0, trMemory());        // Ignore all pre-deps that were copied.

         // Check the thisChild to see if anyone uses this object after the call (if not, we won't add it to post Deps)
         if (callNode->getChild(callNode->getFirstArgumentIndex())->getReferenceCount() > 0)
           postDeps->addPostCondition(RegThis, TR::RealRegister::AssignAny);

         // Add this reg to post deps to ensure no reg motion
         postDeps->addPostConditionIfNotAlreadyInserted(vftReg,  TR::RealRegister::AssignAny);

         bool useCLFIandBRCL = false;

         if (comp()->getOption(TR_enableInterfaceCallCachingSingleDynamicSlot))
            {
            cursor = new (trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::LARL, callNode, snippetReg, ifcSnippet->getDataConstantSnippet(), cg());

            // Single dynamic slot case
            // we cache one class-method pair and atomically load it using LM/LPQ
            TR::Register * classRegister = cg()->allocateRegister();
            TR::Register * methodRegister = cg()->allocateRegister();
            classMethodEPPairRegister = cg()->allocateConsecutiveRegisterPair(methodRegister, classRegister);

            postDeps->addPostCondition(classMethodEPPairRegister, TR::RealRegister::EvenOddPair);
            postDeps->addPostCondition(classRegister, TR::RealRegister::LegalEvenOfPair);
            postDeps->addPostCondition(methodRegister, TR::RealRegister::LegalOddOfPair);

            //Load return address in RegRA
            cursor = new (trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::LARL, callNode, RegRA, returnLocationLabel, cursor, cg());

            if (comp()->target().is64Bit())
               cursor = generateRXInstruction(cg(), TR::InstOpCode::LPQ, callNode, classMethodEPPairRegister,
                        generateS390MemoryReference(snippetReg, ifcSnippet->getDataConstantSnippet()->getSingleDynamicSlotOffset(), cg()), cursor);
            else
               cursor = generateRSInstruction(cg(), TR::InstOpCode::LM, callNode, classMethodEPPairRegister,
                        generateS390MemoryReference(snippetReg, ifcSnippet->getDataConstantSnippet()->getSingleDynamicSlotOffset(), cg()), cursor);

            // We need a dummy label to hook dependencies onto
            cursor = generateS390LabelInstruction(cg(), TR::InstOpCode::label, callNode, paramSetupDummyLabel, preDeps, cursor);

            //check if cached classPtr matches the receiving object classPtr
            cursor = generateRXInstruction(cg(), TR::InstOpCode::getCmpLogicalOpCode(), callNode, classRegister,
                     generateS390MemoryReference(RegThis, 0, cg()), cursor);

            //Cache hit? then jumpto cached method entrypoint directly
            cursor = generateS390RegInstruction(cg(), TR::InstOpCode::BCR, callNode, methodRegister, cursor);
            ((TR::S390RegInstruction *)cursor)->setBranchCondition(TR::InstOpCode::COND_BER);

            cursor = new (trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::LARL, callNode, snippetReg, ifcSnippet,cursor, cg());

            // Cache miss... Too bad.. go to the slow path through the interface call snippet
            cursor = generateS390RegInstruction(cg(), TR::InstOpCode::BCR, callNode, snippetReg, cursor);
            ((TR::S390RegInstruction *)cursor)->setBranchCondition(TR::InstOpCode::COND_BCR);

            // Added NOP so that the pattern matching code in jit2itrg icallVMprJavaSendPatchupVirtual
            cursor = new (trHeapMemory()) TR::S390NOPInstruction(TR::InstOpCode::NOP, 2, callNode, cg());
            }
         else
            {
            useCLFIandBRCL = false && (comp()->target().is64Bit() &&  // Support for 64-bit
                                   TR::Compiler->om.generateCompressedObjectHeaders() // Classes are <2GB on CompressedRefs only.
                                   );

            // Load the interface call data snippet pointer to register is required for non-CLFI / BRCL sequence.
            if (!useCLFIandBRCL)
               {
               cursor = new (trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::LARL, callNode, snippetReg, ifcSnippet->getDataConstantSnippet(), cg());
               methodRegister = cg()->allocateRegister();
               }
            else
               {
#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390)
               if (comp()->getOption(TR_EnableRMODE64))
#endif
                  {
                  // Reserve a trampoline for this interface call. Might not be used, but we only
                  // sacrifice a little trampoline space for it (24-bytes).
                  if (methodSymRef->getReferenceNumber() >= TR_S390numRuntimeHelpers)
                     fej9->reserveTrampolineIfNecessary(comp(), methodSymRef, false);
                  }
#endif
               }

            // 64 bit MultiSlot case

            cursor = generateRILInstruction(cg(), TR::InstOpCode::LARL, callNode, RegRA, returnLocationLabel, cursor);

            // We need a dummy label to hook dependencies.
            cursor = generateS390LabelInstruction(cg(), TR::InstOpCode::label, callNode, paramSetupDummyLabel, preDeps, cursor);

            if (useCLFIandBRCL)
               {
               // Update the IFC Snippet to note we are using CLFI/BRCL sequence.
               // This changes the format of the constants in the data snippet
               ifcSnippet->setUseCLFIandBRCL(true);

               // We will generate CLFI / BRCL sequence to dispatch to target branches.
               // First CLFI/BRCL
               cursor = generateRILInstruction(cg(), TR::InstOpCode::CLFI, callNode, vftReg, 0x0, cursor); //compare against 0

               ifcSnippet->getDataConstantSnippet()->setFirstCLFI(cursor);

               // BRCL
               cursor = generateRILInstruction(cg(), TR::InstOpCode::BRCL, callNode, static_cast<uint32_t>(0x0), reinterpret_cast<void*>(0x0), cursor);

               for(i = 1; i < numInterfaceCallCacheSlots; i++)
                  {
                  // We will generate CLFI / BRCL sequence to dispatch to target branches.
                  cursor = generateRILInstruction(cg(), TR::InstOpCode::CLFI, callNode, vftReg, 0x0, cursor); //compare against 0

                  // BRCL
                  cursor = generateRILInstruction(cg(), TR::InstOpCode::BRCL, callNode, static_cast<uint32_t>(0x0), reinterpret_cast<void*>(0x0), cursor);
                  }
               }
            else
               {
               int32_t slotOffset = ifcSnippet->getDataConstantSnippet()->getFirstSlotOffset();
               for(i = 0; i < numInterfaceCallCacheSlots; i++)
                  {
                  TR::InstOpCode::Mnemonic cmpOp = TR::InstOpCode::getCmpLogicalOpCode();
                  if (comp()->target().is64Bit() && TR::Compiler->om.generateCompressedObjectHeaders())
                     cmpOp = TR::InstOpCode::CL;

                  //check if cached class matches the receiving object class
                  cursor = generateRXInstruction(cg(), cmpOp, callNode, vftReg,
                        generateS390MemoryReference(snippetReg, slotOffset, cg()), cursor);

                  //load cached methodEP from current cache slot
                  cursor = generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), callNode, methodRegister,
                        generateS390MemoryReference(snippetReg, slotOffset+TR::Compiler->om.sizeofReferenceAddress(), cg()), cursor);

                  cursor = generateS390RegInstruction(cg(), TR::InstOpCode::BCR, callNode, methodRegister, cursor);
                  ((TR::S390RegInstruction *)cursor)->setBranchCondition(TR::InstOpCode::COND_BER);

                  slotOffset += 2*TR::Compiler->om.sizeofReferenceAddress();
                  }
               }

            cursor = new (trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::LARL, callNode, snippetReg, ifcSnippet,cursor, cg());

            // Cache miss... Too bad.. go to the slow path through the interface call snippet
            cursor = generateS390RegInstruction(cg(), TR::InstOpCode::BCR, callNode, snippetReg, cursor);
            ((TR::S390RegInstruction *)cursor)->setBranchCondition(TR::InstOpCode::COND_BCR);

            cursor = generateS390LabelInstruction(cg(), TR::InstOpCode::dd, callNode,
               ifcSnippet->getDataConstantSnippet()->getSnippetLabel());

            // Added NOP so that the pattern matching code in jit2itrg icallVMprJavaSendPatchupVirtual
            cursor = new (trHeapMemory()) TR::S390NOPInstruction(TR::InstOpCode::NOP, 2, callNode, cg());

            if (!useCLFIandBRCL)
              postDeps->addPostCondition(methodRegister, TR::RealRegister::AssignAny);
            }

         gcPoint = cursor;
         ((TR::S390CallSnippet *) ifcSnippet)->setBranchInstruction(gcPoint);

         cursor = generateS390LabelInstruction(cg(), TR::InstOpCode::label, callNode, returnLocationLabel, postDeps);

         if (comp()->getOption(TR_enableInterfaceCallCachingSingleDynamicSlot))
            {
            cg()->stopUsingRegister(classMethodEPPairRegister);
            }
         else
            {
            if (!useCLFIandBRCL)
               cg()->stopUsingRegister(methodRegister);
            }
         }
      }
   else if (methodSymbol->isComputed())
      {
      TR::Register *targetAddress = cg()->evaluate(callNode->getFirstChild());
      if (targetAddress->getRegisterPair())
         targetAddress=targetAddress->getRegisterPair()->getLowOrder(); // on 31-bit, the top half doesn't matter, so discard it
      TR::Register *RegRA         = dependencies->searchPostConditionRegister(getReturnAddressRegister());

      gcPoint = generateRRInstruction(cg(), TR::InstOpCode::BASR, callNode, RegRA, targetAddress, dependencies);
      }
   else
      {
      TR_ASSERT(0, "Unknown methodSymbol kind");
      }

   if (cg()->getSupportsRuntimeInstrumentation())
      TR::TreeEvaluator::generateRuntimeInstrumentationOnOffSequence(cg(), TR::InstOpCode::RION, callNode);

   TR_ASSERT( gcPoint, "Expected GC point for a virtual dispatch");
   gcPoint->setNeedsGCMap(getPreservedRegisterMapForGC());
   }

TR::Instruction *
J9::Z::PrivateLinkage::buildDirectCall(TR::Node * callNode, TR::SymbolReference * callSymRef,
   TR::RegisterDependencyConditions * dependencies, int32_t argSize)
   {
   TR::Instruction * gcPoint = NULL;
   TR::MethodSymbol * callSymbol = callSymRef->getSymbol()->castToMethodSymbol();
   TR::ResolvedMethodSymbol * sym = callSymbol->getResolvedMethodSymbol();
   TR_ResolvedMethod * fem = (sym == NULL) ? NULL : sym->getResolvedMethod();
   bool myself;
   bool isJitInduceOSR = callSymRef->isOSRInductionHelper();
   myself = comp()->isRecursiveMethodTarget(fem);

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());

#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390)
   // Reserve a trampoline for this direct call only if it is not a helper call.  It may not be used, but we only
   // sacrifice a little trampoline space for it.
   if (comp()->getOption(TR_EnableRMODE64))
#endif
      {
      if (callSymRef->getReferenceNumber() >= TR_S390numRuntimeHelpers)
         {
         fej9->reserveTrampolineIfNecessary(comp(), callSymRef, false);
         }
      }
#endif

   if (comp()->getOption(TR_TraceCG))
      traceMsg(comp(), "Build Direct Call\n");

   // generate call
   if (isJitInduceOSR)
      {
      TR::LabelSymbol * snippetLabel = generateLabelSymbol(cg());
      TR::LabelSymbol * reStartLabel = generateLabelSymbol(cg());

      gcPoint = generateS390BranchInstruction(cg(), TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, callNode, snippetLabel, dependencies);
      TR::Snippet * snippet = new (trHeapMemory()) TR::S390HelperCallSnippet(cg(), callNode, snippetLabel,
                                                          callSymRef?callSymRef:callNode->getSymbolReference(), reStartLabel, argSize);
      cg()->addSnippet(snippet);

      auto* reStartInstruction = generateS390LabelInstruction(cg(), TR::InstOpCode::label, callNode, reStartLabel);

      // NOP is necessary due to confusion when resolving shared slots at a transition. The OSR infrastructure needs
      // to locate the GC map metadata for this transition point by examining the return address. The algorithm used
      // attempts to find the last instruction PC that is smaller than or equal to the return address. The reason we
      // do this is because under involuntary OSR we may generate the GC map on the return instruction itself. Several
      // of our snippets do this. As such we need to handle both cases, i.e. locating the GC map if its on the yield
      // point or if its on the return address. Hence a less than or equal to comparison is used. We insert this NOP
      // to avoid confusion as the instruction following this yield could also have a GC map registered and we must
      // ensure we pick up the correct metadata.
      cg()->insertPad(callNode, reStartInstruction, 2, false);

      gcPoint->setNeedsGCMap(getPreservedRegisterMapForGC());

      return gcPoint;
      }

   if (!callSymRef->isUnresolved() && !callSymbol->isInterpreted() && ((comp()->compileRelocatableCode() && callSymbol->isHelper()) || !comp()->compileRelocatableCode()))
      {
      // direct call for resolved method

      gcPoint = generateDirectCall(cg(), callNode, myself ? true : false, callSymRef, dependencies);
      gcPoint->setDependencyConditions(dependencies);

      }
   else
      {
      if (cg()->getSupportsRuntimeInstrumentation())
         TR::TreeEvaluator::generateRuntimeInstrumentationOnOffSequence(cg(), TR::InstOpCode::RIOFF, callNode);

      // call through snippet if the method is not resolved or not jitted yet
      TR::LabelSymbol * label = generateLabelSymbol(cg());
      TR::Snippet * snippet;

      if (callSymRef->isUnresolved() || (comp()->compileRelocatableCode() && !comp()->getOption(TR_UseSymbolValidationManager)))
         {
         snippet = new (trHeapMemory()) TR::S390UnresolvedCallSnippet(cg(), callNode, label, argSize);
         }
      else
         {
         snippet = new (trHeapMemory()) TR::S390J9CallSnippet(cg(), callNode, label, callSymRef, argSize);
         }

      cg()->addSnippet(snippet);


      gcPoint = generateSnippetCall(cg(), callNode, snippet, dependencies, callSymRef);

      if (cg()->getSupportsRuntimeInstrumentation())
         TR::TreeEvaluator::generateRuntimeInstrumentationOnOffSequence(cg(), TR::InstOpCode::RION, callNode);
      }

   if (comp()->getOption(TR_TraceCG))
      traceMsg(comp(), "\nGC Point at %p has preserved register map %x\n", gcPoint, getPreservedRegisterMapForGC());

   gcPoint->setNeedsGCMap(getPreservedRegisterMapForGC());
   return gcPoint;
   }


void
J9::Z::PrivateLinkage::callPreJNICallOffloadCheck(TR::Node * callNode)
   {
   TR::CodeGenerator * codeGen = cg();
   TR::LabelSymbol * offloadOffRestartLabel = generateLabelSymbol(codeGen);
   TR::LabelSymbol * offloadOffSnippetLabel = generateLabelSymbol(codeGen);
   TR::SymbolReference * offloadOffSymRef = codeGen->symRefTab()->findOrCreateRuntimeHelper(TR_S390jitPreJNICallOffloadCheck);

   TR::Instruction *gcPoint = generateS390BranchInstruction(
      codeGen, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, callNode, offloadOffSnippetLabel);
   gcPoint->setNeedsGCMap(0);

   codeGen->addSnippet(new (trHeapMemory()) TR::S390HelperCallSnippet(codeGen, callNode,
      offloadOffSnippetLabel, offloadOffSymRef, offloadOffRestartLabel));
   generateS390LabelInstruction(codeGen, TR::InstOpCode::label, callNode, offloadOffRestartLabel);
   }

void
J9::Z::PrivateLinkage::callPostJNICallOffloadCheck(TR::Node * callNode)
   {
   TR::CodeGenerator * codeGen = cg();
   TR::LabelSymbol * offloadOnRestartLabel = generateLabelSymbol(codeGen);
   TR::LabelSymbol * offloadOnSnippetLabel = generateLabelSymbol(codeGen);

   TR::Instruction *gcPoint = generateS390BranchInstruction(
      codeGen, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, callNode, offloadOnSnippetLabel);
   gcPoint->setNeedsGCMap(0);
   TR::SymbolReference * offloadOnSymRef = codeGen->symRefTab()->findOrCreateRuntimeHelper(TR_S390jitPostJNICallOffloadCheck);
   codeGen->addSnippet(new (trHeapMemory()) TR::S390HelperCallSnippet(codeGen, callNode,
      offloadOnSnippetLabel, offloadOnSymRef, offloadOnRestartLabel));
   generateS390LabelInstruction(codeGen, TR::InstOpCode::label, callNode, offloadOnRestartLabel);
   }

void J9::Z::PrivateLinkage::collapseJNIReferenceFrame(TR::Node * callNode,
   TR::RealRegister * javaStackPointerRealRegister,
   TR::Register * javaLitPoolVirtualRegister,
   TR::Register * tempReg)
   {
   // must check to see if the ref pool was used and clean them up if so--or we
   // leave a bunch of pinned garbage behind that screws up the gc quality forever
   TR::CodeGenerator * codeGen = cg();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   intptr_t flagValue = fej9->constJNIReferenceFrameAllocatedFlags();
   TR::LabelSymbol * refPoolRestartLabel = generateLabelSymbol(codeGen);
   TR::LabelSymbol * refPoolSnippetLabel = generateLabelSymbol(codeGen);

   genLoadAddressConstant(codeGen, callNode, flagValue, tempReg, NULL, NULL, javaLitPoolVirtualRegister);

   generateRXInstruction(codeGen, TR::InstOpCode::getAndOpCode(), callNode, tempReg,
      new (trHeapMemory()) TR::MemoryReference(javaStackPointerRealRegister, (int32_t)fej9->constJNICallOutFrameFlagsOffset(), codeGen));
   TR::Instruction *gcPoint =
      generateS390BranchInstruction(codeGen, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, callNode, refPoolSnippetLabel);
   gcPoint->setNeedsGCMap(0);

   TR::SymbolReference * collapseSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_S390collapseJNIReferenceFrame);
   codeGen->addSnippet(new (trHeapMemory()) TR::S390HelperCallSnippet(codeGen, callNode,
      refPoolSnippetLabel, collapseSymRef, refPoolRestartLabel));
   generateS390LabelInstruction(cg(), TR::InstOpCode::label, callNode, refPoolRestartLabel);
   }

//JNI Callout frame
//
//       |-----|
//       |     |      <-- constJNICallOutFrameSpecialTag() (For jni thunk, constJNICallOutFrameInvisibleTag())
// 16/32 |-----|
//       |     |      <-- savedPC ( we don't save anything here
// 12/24 |-----|
//       |     |      <-- return address for JNI call
//  8/16 |-----|
//       |     |      <-- constJNICallOutFrameFlags()
//  4/8   -----
//       |     |      <-- ramMethod for the native method
//        -----       <-- stack pointer
//

// release vm access - use hardware registers because of the control flow
// At this point: arguments for the native routine are all in place already, i.e., if there are
//                more than 24 byte worth of arguments, some of them are on the stack. However,
//                we potentially go out to call a helper before jumping to the native.
//                but the helper call saves and restores all regs
void
J9::Z::PrivateLinkage::setupJNICallOutFrame(TR::Node * callNode,
   TR::RealRegister * javaStackPointerRealRegister,
   TR::Register * methodMetaDataVirtualRegister,
   TR::LabelSymbol * returnFromJNICallLabel,
   TR::S390JNICallDataSnippet *jniCallDataSnippet)
   {
   TR::CodeGenerator * codeGen = cg();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   TR::ResolvedMethodSymbol * cs = callNode->getSymbol()->castToResolvedMethodSymbol();
   TR_ResolvedMethod * resolvedMethod = cs->getResolvedMethod();
   TR::Instruction * cursor = NULL;

   int32_t stackAdjust = (-5 * (int32_t)sizeof(intptr_t));

   cursor = generateRXInstruction(codeGen, TR::InstOpCode::LAY, callNode, javaStackPointerRealRegister, generateS390MemoryReference(javaStackPointerRealRegister, stackAdjust, codeGen), cursor);

   setOffsetToLongDispSlot( getOffsetToLongDispSlot() - stackAdjust );


   // set up Java Thread
   intptr_t constJNICallOutFrameType = fej9->constJNICallOutFrameType();
   TR_ASSERT( constJNICallOutFrameType < MAX_IMMEDIATE_VAL, "OMR::Z::Linkage::setupJNICallOutFrame constJNICallOutFrameType is too big for MVHI");

   TR_ASSERT((fej9->thisThreadGetJavaFrameFlagsOffset() == fej9->thisThreadGetJavaLiteralsOffset() + TR::Compiler->om.sizeofReferenceAddress()) &&
           fej9->thisThreadGetJavaLiteralsOffset() == fej9->thisThreadGetJavaPCOffset() + TR::Compiler->om.sizeofReferenceAddress()
           , "The vmthread field order should be pc,literals,jitStackFrameFlags\n");

   jniCallDataSnippet->setPC(constJNICallOutFrameType);
   jniCallDataSnippet->setLiterals(0);
   jniCallDataSnippet->setJitStackFrameFlags(0);

   generateSS1Instruction(cg(), TR::InstOpCode::MVC, callNode, 3*(TR::Compiler->om.sizeofReferenceAddress()) - 1,
         new (trHeapMemory()) TR::MemoryReference(methodMetaDataVirtualRegister, fej9->thisThreadGetJavaPCOffset(), codeGen),
         new (trHeapMemory()) TR::MemoryReference(jniCallDataSnippet->getBaseRegister(), jniCallDataSnippet->getPCOffset(), codeGen));

   // store out jsp
   generateRXInstruction(codeGen, TR::InstOpCode::getStoreOpCode(), callNode, javaStackPointerRealRegister,
     new (trHeapMemory()) TR::MemoryReference(methodMetaDataVirtualRegister,
       fej9->thisThreadGetJavaSPOffset(), codeGen));

   // JNI Callout Frame setup
   // 0(sp) : RAM method for the native
   intptr_t ramMethod = (uintptr_t) resolvedMethod->resolvedMethodAddress();
   jniCallDataSnippet->setRAMMethod(ramMethod);

   // 4[8](sp) : flags
   intptr_t flags = fej9->constJNICallOutFrameFlags();
   jniCallDataSnippet->setJNICallOutFrameFlags(flags);

   // 8[16](sp) : return address (savedCP)
   jniCallDataSnippet->setReturnFromJNICall(returnFromJNICallLabel);

   // 12[24](sp) : savedPC
   jniCallDataSnippet->setSavedPC(0);

   // 16[32](sp) : tag bits (savedA0)
   intptr_t tagBits = fej9->constJNICallOutFrameSpecialTag();
   // if the current method is simply a wrapper for the JNI call, hide the call-out stack frame
   if (resolvedMethod == comp()->getCurrentMethod())
      {
      tagBits |= fej9->constJNICallOutFrameInvisibleTag();
      }

   jniCallDataSnippet->setTagBits(tagBits);

   generateSS1Instruction(cg(), TR::InstOpCode::MVC, callNode, -stackAdjust - 1,
         new (trHeapMemory()) TR::MemoryReference(javaStackPointerRealRegister, 0, codeGen),
         new (trHeapMemory()) TR::MemoryReference(jniCallDataSnippet->getBaseRegister(), jniCallDataSnippet->getJNICallOutFrameDataOffset(), codeGen));

   }


/**
 * release vm access - use hardware registers because of the control flow
 * At this point: arguments for the native routine are all in place already, i.e., if there are
 *                more than 24 byte worth of arguments, some of them are on the stack. However,
 *                we potentially go out to call a helper before jumping to the native.
 *                but the helper call saves and restores all regs
 */
void J9::Z::JNILinkage::releaseVMAccessMask(TR::Node * callNode,
   TR::Register * methodMetaDataVirtualRegister, TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg,
   TR::S390JNICallDataSnippet * jniCallDataSnippet, TR::RegisterDependencyConditions * deps)
   {
   TR::LabelSymbol * loopHead = generateLabelSymbol(self()->cg());
   TR::LabelSymbol * longReleaseLabel = generateLabelSymbol(self()->cg());
   TR::LabelSymbol * longReleaseSnippetLabel = generateLabelSymbol(self()->cg());
   TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(self()->cg());
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(self()->fe());

   intptr_t aValue = fej9->constReleaseVMAccessMask(); //0xfffffffffffdffdf
   jniCallDataSnippet->setConstReleaseVMAccessMask(aValue);

   generateRXInstruction(self()->cg(), TR::InstOpCode::getLoadOpCode(), callNode, methodAddressReg,
     generateS390MemoryReference(methodMetaDataVirtualRegister,
       fej9->thisThreadGetPublicFlagsOffset(), self()->cg()));


   generateS390LabelInstruction(self()->cg(), TR::InstOpCode::label, callNode, loopHead);
   loopHead->setStartInternalControlFlow();


   aValue = fej9->constReleaseVMAccessOutOfLineMask(); //0x340001
   jniCallDataSnippet->setConstReleaseVMAccessOutOfLineMask(aValue);

   generateRRInstruction(self()->cg(), TR::InstOpCode::getLoadRegOpCode(), callNode, javaLitOffsetReg, methodAddressReg);
   generateRXInstruction(self()->cg(), TR::InstOpCode::getAndOpCode(), callNode, javaLitOffsetReg,
        generateS390MemoryReference(jniCallDataSnippet->getBaseRegister(), jniCallDataSnippet->getConstReleaseVMAccessOutOfLineMaskOffset(), self()->cg()));

   TR::Instruction * gcPoint = (TR::Instruction *) generateS390BranchInstruction(
      self()->cg(), TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, callNode, longReleaseSnippetLabel);
   gcPoint->setNeedsGCMap(0);

   generateRRInstruction(self()->cg(), TR::InstOpCode::getLoadRegOpCode(), callNode, javaLitOffsetReg, methodAddressReg);
   generateRXInstruction(self()->cg(), TR::InstOpCode::getAndOpCode(), callNode, javaLitOffsetReg,
         generateS390MemoryReference(jniCallDataSnippet->getBaseRegister(), jniCallDataSnippet->getConstReleaseVMAccessMaskOffset(), self()->cg()));
   generateRSInstruction(self()->cg(), TR::InstOpCode::getCmpAndSwapOpCode(), callNode, methodAddressReg, javaLitOffsetReg,
     generateS390MemoryReference(methodMetaDataVirtualRegister,
       fej9->thisThreadGetPublicFlagsOffset(), self()->cg()));


   //get existing post conditions on the registers parameters and create a new post cond for the internal control flow
   TR::RegisterDependencyConditions * postDeps = new (self()->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, self()->cg());
   TR::RealRegister::RegNum realReg;
   int32_t regPos = deps->searchPostConditionRegisterPos(methodMetaDataVirtualRegister);
   if (regPos >= 0)
      {
      realReg = deps->getPostConditions()->getRegisterDependency(regPos)->getRealRegister();
      postDeps->addPostCondition(methodMetaDataVirtualRegister, realReg);
      }
   else
      postDeps->addPostCondition(methodMetaDataVirtualRegister, TR::RealRegister::AssignAny);

   regPos = deps->searchPostConditionRegisterPos(methodAddressReg);
   if (regPos >= 0)
      {
      realReg = deps->getPostConditions()->getRegisterDependency(regPos)->getRealRegister();
      postDeps->addPostCondition(methodAddressReg, realReg);
      }
   else
      postDeps->addPostCondition(methodAddressReg, TR::RealRegister::AssignAny);

   regPos = deps->searchPostConditionRegisterPos(javaLitOffsetReg);
   if (regPos >= 0)
      {
      realReg = deps->getPostConditions()->getRegisterDependency(regPos)->getRealRegister();
      postDeps->addPostCondition(javaLitOffsetReg, realReg);
      }
   else
      postDeps->addPostCondition(javaLitOffsetReg, TR::RealRegister::AssignAny);


   generateS390BranchInstruction(self()->cg(), TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, callNode, loopHead);

   generateS390LabelInstruction(self()->cg(), TR::InstOpCode::label, callNode, cFlowRegionEnd, postDeps);
   cFlowRegionEnd->setEndInternalControlFlow();


   self()->cg()->addSnippet(new (self()->trHeapMemory()) TR::S390HelperCallSnippet(self()->cg(), callNode, longReleaseSnippetLabel,
                              comp()->getSymRefTab()->findOrCreateReleaseVMAccessSymbolRef(comp()->getJittedMethodSymbol()), cFlowRegionEnd));
   // end of release vm access (spin lock)
   }


void J9::Z::JNILinkage::acquireVMAccessMask(TR::Node * callNode, TR::Register * javaLitPoolVirtualRegister,
   TR::Register * methodMetaDataVirtualRegister, TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg)
   {
   // start of acquire vm access

   //  WARNING:
   //  As java stack is not yet restored , Make sure that no instruction in this function
   // should use stack.
   // If instruction uses literal pool, it must only be to do load, and such instruction's memory reference should be marked MemRefMustNotSpill
   // so that in case of long disp, we will reuse the target reg as a scratch reg

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(self()->fe());
   intptr_t aValue = fej9->constAcquireVMAccessOutOfLineMask();

   TR::Instruction * loadInstr = (TR::Instruction *) genLoadAddressConstant(self()->cg(), callNode, aValue, methodAddressReg, NULL, NULL, javaLitPoolVirtualRegister);
   switch (loadInstr->getKind())
         {
         case TR::Instruction::IsRX:
         case TR::Instruction::IsRXE:
         case TR::Instruction::IsRXY:
         case TR::Instruction::IsRXYb:
              ((TR::S390RXInstruction *)loadInstr)->getMemoryReference()->setMemRefMustNotSpill();
              break;
         default:
              break;
         }

   generateRRInstruction(self()->cg(), TR::InstOpCode::getXORRegOpCode(), callNode, javaLitOffsetReg, javaLitOffsetReg);

   TR::LabelSymbol * longAcquireLabel = generateLabelSymbol(self()->cg());
   TR::LabelSymbol * longAcquireSnippetLabel = generateLabelSymbol(self()->cg());
   TR::LabelSymbol * acquireDoneLabel = generateLabelSymbol(self()->cg());
   generateRSInstruction(cg(), TR::InstOpCode::getCmpAndSwapOpCode(), callNode, javaLitOffsetReg, methodAddressReg,
      generateS390MemoryReference(methodMetaDataVirtualRegister,
         (int32_t)fej9->thisThreadGetPublicFlagsOffset(), self()->cg()));
   TR::Instruction *gcPoint = (TR::Instruction *) generateS390BranchInstruction(self()->cg(), TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, callNode, longAcquireSnippetLabel);
   gcPoint->setNeedsGCMap(0);

   self()->cg()->addSnippet(new (self()->trHeapMemory()) TR::S390HelperCallSnippet(self()->cg(), callNode, longAcquireSnippetLabel,
                              comp()->getSymRefTab()->findOrCreateAcquireVMAccessSymbolRef(comp()->getJittedMethodSymbol()), acquireDoneLabel));
   generateS390LabelInstruction(self()->cg(), TR::InstOpCode::label, callNode, acquireDoneLabel);
   // end of acquire vm accessa
   }

#ifdef J9VM_INTERP_ATOMIC_FREE_JNI

/**
 * \brief
 * Build the atomic-free release VM access sequence for JNI dispatch.
 *
 * \details
 * This is the atomic-free JNI design and works in conjunction with VMAccess.cpp atomic-free JNI changes.
 *
 * In the JNI dispatch sequence, a release-vm-access action is performed before the branch to native code; and an acquire-vm-access
 * is done after the thread execution returns from the native call. Both of the actions require synchronization between the
 * application thread and the GC thread. This was previously implemented with the atomic compare-and-swap (CS) instruction, which is slow in nature.
 *
 * To speed up the JNI acquire and release access actions (the fast path), a store-load sequence is generated by this evaluator
 * to replace the CS instruction. Normally, the fast path ST-LD are not serialized and can be done out-of-order for higher performance. Synchronization
 * burden is offloaded to the slow path.
 *
 * The slow path is where a thread tries to acquire exclusive vm access. The slow path should be taken proportionally less often than the fast
 * path. Should the slow path be taken, that thread will be penalized by calling a slow flushProcessWriteBuffer() routine so that all threads
 * can momentarily synchronize memory writes. Having fast and slow paths makes the atomic-free JNI design asymmetric.
 *
 * Note that the z/OS currently does not support the asymmetric algorithm. Hence, a serialization instruction is required between the
 * store and the load.
 *
*/
void
J9::Z::JNILinkage::releaseVMAccessMaskAtomicFree(TR::Node * callNode,
                                                    TR::Register * methodMetaDataVirtualRegister,
                                                    TR::Register * tempReg1)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe();
   TR::CodeGenerator* cg = self()->cg();

   // Store a 1 into vmthread->inNative
   generateSILInstruction(cg, TR::InstOpCode::getMoveHalfWordImmOpCode(), callNode,
                        generateS390MemoryReference(methodMetaDataVirtualRegister, offsetof(J9VMThread, inNative), cg),
                        1);


#if !defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH)
   generateSerializationInstruction(cg, callNode, NULL);
#endif

   // Compare vmthread public flag with J9_PUBLIC_FLAGS_VM_ACCESS
   generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), callNode, tempReg1,
                         generateS390MemoryReference(methodMetaDataVirtualRegister, fej9->thisThreadGetPublicFlagsOffset(), cg));

   TR::LabelSymbol * longReleaseSnippetLabel = generateLabelSymbol(cg);
   TR::LabelSymbol * longReleaseRestartLabel = generateLabelSymbol(cg);

   TR_ASSERT_FATAL(J9_PUBLIC_FLAGS_VM_ACCESS >= MIN_IMMEDIATE_BYTE_VAL && J9_PUBLIC_FLAGS_VM_ACCESS <= MAX_IMMEDIATE_BYTE_VAL, "VM access bit must be immediate");

   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), callNode, tempReg1, J9_PUBLIC_FLAGS_VM_ACCESS, TR::InstOpCode::COND_BNE, longReleaseSnippetLabel, false);

   cg->addSnippet(new (self()->trHeapMemory()) TR::S390HelperCallSnippet(cg,
                                                                         callNode, longReleaseSnippetLabel,
                                                                         comp()->getSymRefTab()->findOrCreateReleaseVMAccessSymbolRef(comp()->getJittedMethodSymbol()),
                                                                         longReleaseRestartLabel));

   generateS390LabelInstruction(cg, TR::InstOpCode::label, callNode, longReleaseRestartLabel);
   }

/**
 * \brief
 * Build the atomic-free acquire VM access sequence for JNI dispatch.
 *
 * */
void
J9::Z::JNILinkage::acquireVMAccessMaskAtomicFree(TR::Node * callNode,
                                                    TR::Register * methodMetaDataVirtualRegister,
                                                    TR::Register * tempReg1)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe();
   TR::CodeGenerator* cg = self()->cg();

   // Zero vmthread->inNative, which is a UDATA field
   generateSS1Instruction(cg, TR::InstOpCode::XC, callNode, TR::Compiler->om.sizeofReferenceAddress() - 1,
                          generateS390MemoryReference(methodMetaDataVirtualRegister, offsetof(J9VMThread, inNative), cg),
                          generateS390MemoryReference(methodMetaDataVirtualRegister, offsetof(J9VMThread, inNative), cg));

#if !defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH)
   generateSerializationInstruction(cg, callNode, NULL);
#endif

   // Compare vmthread public flag with J9_PUBLIC_FLAGS_VM_ACCESS
   generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), callNode, tempReg1,
                         generateS390MemoryReference(methodMetaDataVirtualRegister, fej9->thisThreadGetPublicFlagsOffset(), cg));

   TR::LabelSymbol * longAcquireSnippetLabel = generateLabelSymbol(cg);
   TR::LabelSymbol * longAcquireRestartLabel = generateLabelSymbol(cg);

   TR_ASSERT_FATAL(J9_PUBLIC_FLAGS_VM_ACCESS >= MIN_IMMEDIATE_BYTE_VAL && J9_PUBLIC_FLAGS_VM_ACCESS <= MAX_IMMEDIATE_BYTE_VAL, "VM access bit must be immediate");

   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), callNode, tempReg1, J9_PUBLIC_FLAGS_VM_ACCESS, TR::InstOpCode::COND_BNE, longAcquireSnippetLabel, false);

   cg->addSnippet(new (self()->trHeapMemory()) TR::S390HelperCallSnippet(cg,
                                                                         callNode, longAcquireSnippetLabel,
                                                                         comp()->getSymRefTab()->findOrCreateAcquireVMAccessSymbolRef(comp()->getJittedMethodSymbol()),
                                                                         longAcquireRestartLabel));

   generateS390LabelInstruction(cg, TR::InstOpCode::label, callNode, longAcquireRestartLabel);
   }
#endif

void J9::Z::JNILinkage::checkException(TR::Node * callNode,
   TR::Register * methodMetaDataVirtualRegister,
   TR::Register * tempReg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   // check exception
   TR::LabelSymbol * exceptionRestartLabel = generateLabelSymbol(self()->cg());
   TR::LabelSymbol * exceptionSnippetLabel = generateLabelSymbol(self()->cg());
   generateRXInstruction(self()->cg(), TR::InstOpCode::getLoadOpCode(), callNode, tempReg,
               new (self()->trHeapMemory()) TR::MemoryReference(methodMetaDataVirtualRegister, fej9->thisThreadGetCurrentExceptionOffset(), self()->cg()));

   TR::Instruction *gcPoint = generateS390CompareAndBranchInstruction(self()->cg(),
      TR::InstOpCode::getCmpOpCode(), callNode, tempReg, 0, TR::InstOpCode::COND_BNE, exceptionSnippetLabel, false, true);
   gcPoint->setNeedsGCMap(0);

   self()->cg()->addSnippet(new (self()->trHeapMemory()) TR::S390HelperCallSnippet(self()->cg(), callNode, exceptionSnippetLabel,
      comp()->getSymRefTab()->findOrCreateThrowCurrentExceptionSymbolRef(comp()->getJittedMethodSymbol()), exceptionRestartLabel));
   generateS390LabelInstruction(self()->cg(), TR::InstOpCode::label, callNode, exceptionRestartLabel);
   }

void
J9::Z::JNILinkage::processJNIReturnValue(TR::Node * callNode,
                                            TR::CodeGenerator* cg,
                                            TR::Register* javaReturnRegister)
   {
   auto resolvedMethod = callNode->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod();
   auto returnType = resolvedMethod->returnType();
   const bool isUnwrapAddressReturnValue = !((TR_J9VMBase *)fe())->jniDoNotWrapObjects(resolvedMethod)
                                            && (returnType == TR::Address);

   TR::LabelSymbol *cFlowRegionStart = NULL, *cFlowRegionEnd = NULL;

   if (isUnwrapAddressReturnValue)
      {
      cFlowRegionStart = generateLabelSymbol(cg);
      cFlowRegionEnd = generateLabelSymbol(cg);

      generateS390LabelInstruction(cg, TR::InstOpCode::label, callNode, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), callNode, javaReturnRegister, 0, TR::InstOpCode::COND_BE, cFlowRegionEnd);
      generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), callNode, javaReturnRegister,
                            generateS390MemoryReference(javaReturnRegister, 0, cg));

      generateS390LabelInstruction(cg, TR::InstOpCode::label, callNode, cFlowRegionEnd);
      cFlowRegionEnd->setEndInternalControlFlow();
      }
   else if ((returnType == TR::Int8) && comp()->getSymRefTab()->isReturnTypeBool(callNode->getSymbolReference()))
      {
      if (comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_Z13))
         {
         generateRIInstruction(cg, TR::InstOpCode::getCmpHalfWordImmOpCode(), callNode, javaReturnRegister, 0);
         generateRIEInstruction(cg, comp()->target().is64Bit() ? TR::InstOpCode::LOCGHI : TR::InstOpCode::LOCHI,
                                callNode, javaReturnRegister, 1, TR::InstOpCode::COND_BNE);
         }
      else
         {
         cFlowRegionStart = generateLabelSymbol(cg);
         cFlowRegionEnd = generateLabelSymbol(cg);

         generateS390LabelInstruction(cg, TR::InstOpCode::label, callNode, cFlowRegionStart);
         cFlowRegionStart->setStartInternalControlFlow();
         generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), callNode, javaReturnRegister,
                                                 0, TR::InstOpCode::COND_BE, cFlowRegionEnd);
         generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), callNode, javaReturnRegister, 1);
         generateS390LabelInstruction(cg, TR::InstOpCode::label, callNode, cFlowRegionEnd);
         cFlowRegionEnd->setEndInternalControlFlow();
         }
      }
   }

TR::Register * J9::Z::JNILinkage::buildDirectDispatch(TR::Node * callNode)
   {
   if (comp()->getOption(TR_TraceCG))
      traceMsg(comp(), "\nbuildDirectDispatch\n");

   TR::CodeGenerator * codeGen = cg();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   TR::SystemLinkage * systemLinkage = (TR::SystemLinkage *) cg()->getLinkage(TR_System);
   TR::LabelSymbol * returnFromJNICallLabel = generateLabelSymbol(cg());
   TR::RegisterDependencyConditions * deps;

   // Extra dependency for killing volatile high registers (see KillVolHighRegs)
   int32_t numDeps = systemLinkage->getNumberOfDependencyGPRegisters() + 1;

   if (cg()->getSupportsVectorRegisters())
      numDeps += 32; //VRFs need to be spilled

   // 70896 Remove DEPEND instruction and merge glRegDeps to call deps
   // *Speculatively* increase numDeps for dependencies from glRegDeps
   // which is added right before callNativeFunction.
   // GlobalRegDeps should not add any more children after here.
   TR::RegisterDependencyConditions *glRegDeps;
   TR::Node *GlobalRegDeps;

   bool hasGlRegDeps = (callNode->getNumChildren() >= 1) &&
            (callNode->getChild(callNode->getNumChildren()-1)->getOpCodeValue() == TR::GlRegDeps);
   if(hasGlRegDeps)
      {
      GlobalRegDeps = callNode->getChild(callNode->getNumChildren()-1);
      numDeps += GlobalRegDeps->getNumChildren();
      }

   deps = generateRegisterDependencyConditions(numDeps, numDeps, cg());
   int64_t killMask = -1;
   TR::Register *vftReg = NULL;
   TR::S390JNICallDataSnippet * jniCallDataSnippet = NULL;
   TR::RealRegister * javaStackPointerRealRegister = getStackPointerRealRegister();
   TR::RealRegister * methodMetaDataRealRegister = getMethodMetaDataRealRegister();
   TR::RealRegister * javaLitPoolRealRegister = getLitPoolRealRegister();

   TR::Register * javaLitPoolVirtualRegister = javaLitPoolRealRegister;
   TR::Register * methodMetaDataVirtualRegister = methodMetaDataRealRegister;

   TR::Register * methodAddressReg = NULL;
   TR::Register * javaLitOffsetReg = NULL;
   intptr_t targetAddress = (intptr_t) 0;
   TR::DataType returnType = TR::NoType;
   int8_t numTempRegs = -1;
   comp()->setHasNativeCall();

   if (codeGen->getSupportsRuntimeInstrumentation())
      TR::TreeEvaluator::generateRuntimeInstrumentationOnOffSequence(codeGen, TR::InstOpCode::RIOFF, callNode);

   TR::ResolvedMethodSymbol * cs = callNode->getSymbol()->castToResolvedMethodSymbol();
   TR_ResolvedMethod * resolvedMethod = cs->getResolvedMethod();
   bool isFastJNI = true;
   bool isPassJNIThread = !fej9->jniDoNotPassThread(resolvedMethod);
   bool isPassReceiver = !fej9->jniDoNotPassReceiver(resolvedMethod);
   bool isJNIGCPoint = !fej9->jniNoGCPoint(resolvedMethod);
   bool isJNICallOutFrame = !fej9->jniNoNativeMethodFrame(resolvedMethod);
   bool isReleaseVMAccess = !fej9->jniRetainVMAccess(resolvedMethod);
   bool isJavaOffLoadCheck = false;
   bool isAcquireVMAccess = isReleaseVMAccess;
   bool isCollapseJNIReferenceFrame = !fej9->jniNoSpecialTeardown(resolvedMethod);
   bool isCheckException = !fej9->jniNoExceptionsThrown(resolvedMethod);
   bool isKillAllUnlockedGPRs = isJNIGCPoint;

   killMask = killAndAssignRegister(killMask, deps, &methodAddressReg, (comp()->target().isLinux()) ?  TR::RealRegister::GPR1 : TR::RealRegister::GPR9 , codeGen, true);
   killMask = killAndAssignRegister(killMask, deps, &javaLitOffsetReg, TR::RealRegister::GPR11, codeGen, true);

   targetAddress = (intptr_t) resolvedMethod->startAddressForJNIMethod(comp());
   returnType = resolvedMethod->returnType();

   static char * disablePureFn = feGetEnv("TR_DISABLE_PURE_FUNC_RECOGNITION");
   if (cs->canDirectNativeCall())
      {
      isReleaseVMAccess = false;
      isAcquireVMAccess = false;
      isKillAllUnlockedGPRs = false;
      isJNIGCPoint = false;
      isCheckException = false;
      isJNICallOutFrame = false;
      }
   if (cs->isPureFunction() && (disablePureFn == NULL))
      {
      isReleaseVMAccess=false;
      isAcquireVMAccess=false;
      isCheckException = false;
      }
   if ((fej9->isJavaOffloadEnabled() && static_cast<TR_ResolvedJ9Method *>(resolvedMethod)->methodIsNotzAAPEligible()) || (fej9->CEEHDLREnabled() && isJNICallOutFrame))
      isJavaOffLoadCheck = true;


   if (comp()->getOption(TR_TraceCG))
      traceMsg(comp(), "isPassReceiver: %d, isPassJNIThread: %d, isJNIGCPoint: %d, isJNICallOutFrame:%d, isReleaseVMAccess: %d, isCollapseJNIReferenceFrame: %d, isJNIGCPoint: %d\n", isPassReceiver, isPassJNIThread, isJNIGCPoint, isJNICallOutFrame, isReleaseVMAccess, isCollapseJNIReferenceFrame, isJNIGCPoint);

   if (isPassJNIThread)
      {
      //First param for JNI call in JNIEnv pointer
      TR::Register * jniEnvRegister = cg()->allocateRegister();
      deps->addPreCondition(jniEnvRegister, systemLinkage->getIntegerArgumentRegister(0));
      generateRRInstruction(codeGen, TR::InstOpCode::getLoadRegOpCode(), callNode,
      jniEnvRegister, methodMetaDataVirtualRegister);
      }

   // JNI dispatch does not allow for any object references to survive in preserved registers as they are saved onto
   // the system stack, which the JVM stack walker has no awareness of. Hence we need to ensure that all object
   // references are evicted from preserved registers at the call site.
   TR::Register* tempReg = cg()->allocateRegister();

   deps->addPostCondition(tempReg, TR::RealRegister::KillVolHighRegs);
   cg()->stopUsingRegister(tempReg);

   setupRegisterDepForLinkage(callNode, TR_JNIDispatch, deps, killMask, systemLinkage, GlobalRegDeps, hasGlRegDeps, &methodAddressReg, javaLitOffsetReg);

   setupBuildArgForLinkage(callNode, TR_JNIDispatch, deps, isFastJNI, isPassReceiver, killMask, GlobalRegDeps, hasGlRegDeps, systemLinkage);

   if (isJNICallOutFrame || isReleaseVMAccess)
     {
     TR::Register * JNISnippetBaseReg = NULL;
     killMask = killAndAssignRegister(killMask, deps, &JNISnippetBaseReg, TR::RealRegister::GPR12, codeGen, true);
     jniCallDataSnippet = new (trHeapMemory()) TR::S390JNICallDataSnippet(cg(), callNode);
     cg()->addSnippet(jniCallDataSnippet);
     jniCallDataSnippet->setBaseRegister(JNISnippetBaseReg);
     new (trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::LARL, callNode,
                                     jniCallDataSnippet->getBaseRegister(), jniCallDataSnippet, codeGen);
     jniCallDataSnippet->setTargetAddress(targetAddress);
     }

   if (isJNICallOutFrame)
     {
      // Sets up PC, Stack pointer and literals offset slots.
      setupJNICallOutFrame(callNode, javaStackPointerRealRegister, methodMetaDataVirtualRegister,
                              returnFromJNICallLabel, jniCallDataSnippet);

#if (JAVA_SPEC_VERSION >= 19)
#if defined(TR_TARGET_64BIT)
      /**
       * For virtual threads, bump the callOutCounter.  It is safe and most efficient to
       * do this unconditionally.  No need to check for overflow.
       */
      generateSIInstruction(cg(), TR::InstOpCode::AGSI, callNode, generateS390MemoryReference(methodMetaDataVirtualRegister, fej9->thisThreadGetCallOutCountOffset(), cg()), 1);
#else    /* TR_TARGET_64BIT */
   TR_ASSERT_FATAL(false, "Virtual Thread is not supported on 31-Bit platform\n");
#endif   /* TR_TARGET_64BIT */
#endif   /* JAVA_SPEC_VERSION >= 19 */

     }
   else
     {
     // store java stack pointer
     generateRXInstruction(codeGen, TR::InstOpCode::getStoreOpCode(), callNode, javaStackPointerRealRegister,
                     new (trHeapMemory()) TR::MemoryReference(methodMetaDataVirtualRegister, (int32_t)fej9->thisThreadGetJavaSPOffset(), codeGen));


     auto* literalOffsetMemoryReference = new (trHeapMemory()) TR::MemoryReference(methodMetaDataVirtualRegister, (int32_t)fej9->thisThreadGetJavaLiteralsOffset(), codeGen);

     // Set up literal offset slot to zero
     generateSILInstruction(codeGen, TR::InstOpCode::getMoveHalfWordImmOpCode(), callNode, literalOffsetMemoryReference, 0);
     }

    if (isReleaseVMAccess)
    {
#ifdef J9VM_INTERP_ATOMIC_FREE_JNI
      releaseVMAccessMaskAtomicFree(callNode, methodMetaDataVirtualRegister, methodAddressReg);
#else
      releaseVMAccessMask(callNode, methodMetaDataVirtualRegister, methodAddressReg, javaLitOffsetReg, jniCallDataSnippet, deps);
#endif
     }

   //Turn off Java Offload if calling user native
   if (isJavaOffLoadCheck)
     {
     callPreJNICallOffloadCheck(callNode);
     }

   // Generate a call to the native function
   TR::Register * javaReturnRegister = systemLinkage->callNativeFunction(
      callNode, deps, targetAddress, methodAddressReg, javaLitOffsetReg, returnFromJNICallLabel,
      jniCallDataSnippet, isJNIGCPoint);

     // restore java stack pointer
     generateRXInstruction(codeGen, TR::InstOpCode::getLoadOpCode(), callNode, javaStackPointerRealRegister,
                     new (trHeapMemory()) TR::MemoryReference(methodMetaDataVirtualRegister, (int32_t)fej9->thisThreadGetJavaSPOffset(), codeGen));

   //Turn on Java Offload
   if (isJavaOffLoadCheck)
     {
     callPostJNICallOffloadCheck(callNode);
     }

   if (isAcquireVMAccess)
     {
#ifdef J9VM_INTERP_ATOMIC_FREE_JNI
     acquireVMAccessMaskAtomicFree(callNode, methodMetaDataVirtualRegister, methodAddressReg);
#else
     acquireVMAccessMask(callNode, javaLitPoolVirtualRegister, methodMetaDataVirtualRegister, methodAddressReg, javaLitOffsetReg);
#endif
     }

#if (JAVA_SPEC_VERSION >= 19)
#if defined(TR_TARGET_64BIT)
   if (isJNICallOutFrame)
      {
      /**
       * For virtual threads, decrement the callOutCounter.  It is safe and most efficient to
       * do this unconditionally.  No need to check for underflow.
       */
      generateSIInstruction(cg(), TR::InstOpCode::AGSI, callNode, generateS390MemoryReference(methodMetaDataVirtualRegister, fej9->thisThreadGetCallOutCountOffset(), cg()), -1);
      }
#else    /* TR_TARGET_64BIT */
   TR_ASSERT_FATAL(false, "Virtual Thread is not supported on 31-Bit platform\n");
#endif   /* TR_TARGET_64BIT */
#endif   /* JAVA_SPEC_VERSION >= 19 */

   generateRXInstruction(codeGen, TR::InstOpCode::getAddOpCode(), callNode, javaStackPointerRealRegister,
            new (trHeapMemory()) TR::MemoryReference(methodMetaDataVirtualRegister, (int32_t)fej9->thisThreadGetJavaLiteralsOffset(), codeGen));

   processJNIReturnValue(callNode, codeGen, javaReturnRegister);

   if (isCollapseJNIReferenceFrame)
     {
     collapseJNIReferenceFrame(callNode, javaStackPointerRealRegister, javaLitPoolVirtualRegister, methodAddressReg);
     }

   // Restore the JIT frame
   if (isJNICallOutFrame)
     {
     generateRXInstruction(codeGen, TR::InstOpCode::LA, callNode, javaStackPointerRealRegister,
        generateS390MemoryReference(javaStackPointerRealRegister, 5 * sizeof(intptr_t), codeGen));

     setOffsetToLongDispSlot(getOffsetToLongDispSlot() - (5 * (int32_t)sizeof(intptr_t)) );
     }

   if (isCheckException)
     {
     checkException(callNode, methodMetaDataVirtualRegister, methodAddressReg);
     }

   OMR::Z::Linkage::generateDispatchReturnLable(callNode, codeGen, deps, javaReturnRegister, hasGlRegDeps, GlobalRegDeps);
   return javaReturnRegister;
   }

////////////////////////////////////////////////////////////////////////////////
// J9::Z::PrivateLinkage::doNotKillSpecialRegsForBuildArgs -  Do not kill
// special regs (java stack ptr, system stack ptr, and method metadata reg)
////////////////////////////////////////////////////////////////////////////////
void
J9::Z::PrivateLinkage::doNotKillSpecialRegsForBuildArgs (TR::Linkage *linkage, bool isFastJNI, int64_t &killMask)
   {
   TR::SystemLinkage * systemLinkage = (TR::SystemLinkage *) cg()->getLinkage(TR_System);

   int32_t i;
   killMask &= ~(0x1L << REGINDEX(getStackPointerRegister()));

   if (systemLinkage->getStackPointerRealRegister()->getState() == TR::RealRegister::Locked)
      {
      killMask &= ~(0x1L << REGINDEX(getSystemStackPointerRegister()));
      }
   killMask &= ~(0x1L << REGINDEX(getMethodMetaDataRegister()));

   // Remove preserved registers from kill set
   if (isFastJNI)
      {
      // We kill all unlocked GPRs for JNI preserved or not,
      // so only need to worry about not killing preserved FPRs
      for (i = TR::RealRegister::FirstFPR; i <= TR::RealRegister::LastFPR; i++)
         {
         if (linkage->getPreserved(REGNUM(i)))
            killMask &= ~(0x1L << REGINDEX(i));
         }
      }
   else
      {
      for (i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastFPR; i++)
         {
         if (linkage->getPreserved(REGNUM(i)))
            killMask &= ~(0x1L << REGINDEX(i));
         }
      }
   }

////////////////////////////////////////////////////////////////////////////////
// J9::Z::PrivateLinkage::addSpecialRegDepsForBuildArgs -  add special argument
// register dependencies for buildArgs
////////////////////////////////////////////////////////////////////////////////
void
J9::Z::PrivateLinkage::addSpecialRegDepsForBuildArgs(TR::Node * callNode, TR::RegisterDependencyConditions * dependencies, int32_t& from, int32_t step)
   {
   TR::Node * child;
   TR::RealRegister::RegNum specialArgReg = TR::RealRegister::NoReg;
   switch (callNode->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod())
      {
      // Note: special long args are still only passed in one GPR
      case TR::java_lang_invoke_ComputedCalls_dispatchJ9Method:
         specialArgReg = getJ9MethodArgumentRegister();
         break;
      case TR::java_lang_invoke_ComputedCalls_dispatchVirtual:
      case TR::com_ibm_jit_JITHelpers_dispatchVirtual:
         specialArgReg = getVTableIndexArgumentRegister();
         break;
      }

   if (specialArgReg != TR::RealRegister::NoReg)
      {
      child = callNode->getChild(from);
      TR::Register *specialArg = copyArgRegister(callNode, child, cg()->evaluate(child)); // TODO:JSR292: We don't need a copy of the highOrder reg on 31-bit
      if (specialArg->getRegisterPair())
         specialArg = specialArg->getLowOrder(); // on 31-bit, the top half doesn't matter, so discard it
      dependencies->addPreCondition(specialArg, specialArgReg );
      cg()->decReferenceCount(child);

      if (comp()->getOption(TR_TraceCG))
         {
         traceMsg(comp(), "Special arg %s %s reg %s in %s\n",
            callNode->getOpCode().getName(),
            comp()->getDebug()->getName(callNode->getChild(from)),
            comp()->getDebug()->getName(callNode->getRegister()),
            comp()->getDebug()->getName(cg()->machine()->getRealRegister(specialArgReg)));
         }

      from += step;
      }
   }

////////////////////////////////////////////////////////////////////////////////
// J9::Z::PrivateLinkage::storeExtraEnvRegForBuildArgs -  JNI specific,
// account for extra env param. Return stackOffset.
////////////////////////////////////////////////////////////////////////////////
int32_t
J9::Z::PrivateLinkage::storeExtraEnvRegForBuildArgs(TR::Node * callNode, TR::Linkage* linkage, TR::RegisterDependencyConditions * dependencies,
      bool isFastJNI, int32_t stackOffset, int8_t gprSize, uint32_t &numIntegerArgs)
   {
  //In XPLINK, when the called function has variable number of args, all args are passed on stack,
  //Because we have no way of knowing this, we will always store the args on stack and parm regs both.
   if (isFastJNI)  // Account for extra parameter env
     {
     TR::Register * jniEnvRegister = dependencies->searchPreConditionRegister(getIntegerArgumentRegister(0));
     numIntegerArgs += 1;
     if (linkage->isAllParmsOnStack())
        {
        TR::Register *stackRegister = linkage->getStackRegisterForOutgoingArguments(callNode, dependencies);  // delay (possibly) creating this till needed
        storeArgumentOnStack(callNode, TR::InstOpCode::getStoreOpCode(), jniEnvRegister, &stackOffset, stackRegister);
        }
     if (linkage->isXPLinkLinkageType()) // call specific
        {
        stackOffset += gprSize;
        }
     }
   return stackOffset;
   }

////////////////////////////////////////////////////////////////////////////////
// J9::Z::PrivateLinkage::addFECustomizedReturnRegDependency -  add extra
// linkage specific return register dependency
////////////////////////////////////////////////////////////////////////////////
int64_t
J9::Z::PrivateLinkage::addFECustomizedReturnRegDependency(int64_t killMask, TR::Linkage* linkage, TR::DataType resType,
      TR::RegisterDependencyConditions * dependencies)
   {
   TR::Register * javaResultReg;

   //In zOS XPLink, return register(GPR3) is not same as privateLinkage (GPR2)
   // hence we need to add another dependency
   if (linkage->getIntegerReturnRegister() != getIntegerReturnRegister())
      {
      javaResultReg =  (resType.isAddress())? cg()->allocateCollectedReferenceRegister() : cg()->allocateRegister();
      dependencies->addPostCondition(javaResultReg, getIntegerReturnRegister(),DefinesDependentRegister);
      killMask &= (~(0x1L << REGINDEX(getIntegerReturnRegister())));
      }
   return killMask;
   }

////////////////////////////////////////////////////////////////////////////////
// J9::Z::PrivateLinkage::buildDirectDispatch - build direct function call
//   eg. Static, helpers... etc.
////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::PrivateLinkage::buildDirectDispatch(TR::Node * callNode)
   {
   TR::SymbolReference * callSymRef = callNode->getSymbolReference();
   TR::MethodSymbol * callSymbol = callSymRef->getSymbol()->castToMethodSymbol();
   int32_t argSize;
   TR::Register * returnRegister;
   TR::Register *vftReg = NULL;

   if (comp()->getOption(TR_TraceCG))
      traceMsg(comp(), "\nbuildDirectDispatch\n");

   // create register dependency conditions
   TR::RegisterDependencyConditions * dependencies = generateRegisterDependencyConditions(getNumberOfDependencyGPRegisters(),
                                                           getNumberOfDependencyGPRegisters(), cg());

   // setup arguments
   argSize = buildArgs(callNode, dependencies, false, -1, vftReg);

   buildDirectCall(callNode, callSymRef,  dependencies, argSize);

   // set dependency on return register
   TR::Register * lowReg = NULL, * highReg;
   switch (callNode->getOpCodeValue())
      {
      case TR::icall:
      case TR::acall:
         returnRegister = dependencies->searchPostConditionRegister(getIntegerReturnRegister());
         break;
      case TR::lcall:
            {
            if (comp()->target().is64Bit())
               {
               returnRegister = dependencies->searchPostConditionRegister(getLongReturnRegister());
               }
            else
               {
               TR::Instruction *cursor = NULL;
               lowReg = dependencies->searchPostConditionRegister(getLongLowReturnRegister());
               highReg = dependencies->searchPostConditionRegister(getLongHighReturnRegister());

               generateRSInstruction(cg(), TR::InstOpCode::SLLG, callNode, highReg, highReg, 32);
               cursor =
                  generateRRInstruction(cg(), TR::InstOpCode::LR, callNode, highReg, lowReg);

               TR::RegisterDependencyConditions * deps =
                  new (cg()->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg());
               deps->addPostCondition(lowReg, getLongLowReturnRegister(),DefinesDependentRegister);
               deps->addPostCondition(highReg, getLongHighReturnRegister(),DefinesDependentRegister);
               cursor->setDependencyConditions(deps);

               cg()->stopUsingRegister(lowReg);
               returnRegister = highReg;
               }
            }
         break;
      case TR::fcall:
      case TR::dcall:
         returnRegister = dependencies->searchPostConditionRegister(getFloatReturnRegister());
         break;
      case TR::call:
         returnRegister = NULL;
         break;
      default:
         returnRegister = NULL;
         TR_ASSERT(0, "Unknown direct call Opcode %d.", callNode->getOpCodeValue());
      }

   callNode->setRegister(returnRegister);

#if TODO // for live register - to do later
   cg()->freeAndResetTransientLongs();
#endif
   dependencies->stopUsingDepRegs(cg(), lowReg == NULL ? returnRegister : highReg, lowReg);

   return returnRegister;
   }

////////////////////////////////////////////////////////////////////////////////
// J9::Z::PrivateLinkage::buildIndirectDispatch - build indirect function call.
//   This function handles the arguments setup and the return register. It will
//   buildVirtualDispatch() to handle the call sequence.
////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::PrivateLinkage::buildIndirectDispatch(TR::Node * callNode)
   {
   TR::RegisterDependencyConditions * dependencies = NULL;
   int32_t argSize = 0;
   TR::Register * returnRegister;
   TR::SymbolReference * methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol * methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   TR::Register *vftReg = NULL;
   //TR::S390SystemLinkage * systemLinkage = (TR::S390SystemLinkage *) cg()->getLinkage(TR_System);


   if (comp()->getOption(TR_TraceCG))
      traceMsg(comp(), "\nbuildIndirectDispatch\n");

   // create register dependency conditions
   dependencies = generateRegisterDependencyConditions(getNumberOfDependencyGPRegisters(),
                     getNumberOfDependencyGPRegisters(), cg());

   argSize = buildArgs(callNode, dependencies, false, -1, vftReg);
   buildVirtualDispatch(callNode, dependencies, vftReg, argSize);

   TR::Register * lowReg = NULL, * highReg;
   switch (callNode->getOpCodeValue())
      {
      case TR::icalli:
      case TR::acalli:
         returnRegister = dependencies->searchPostConditionRegister(getIntegerReturnRegister());
         break;
      case TR::lcalli:
            {
            if (comp()->target().is64Bit())
               {
               returnRegister = dependencies->searchPostConditionRegister(getLongReturnRegister());
               }
            else
               {
               TR::Instruction *cursor = NULL;
               lowReg = dependencies->searchPostConditionRegister(getLongLowReturnRegister());
               highReg = dependencies->searchPostConditionRegister(getLongHighReturnRegister());

               generateRSInstruction(cg(), TR::InstOpCode::SLLG, callNode, highReg, highReg, 32);
               cursor =
                  generateRRInstruction(cg(), TR::InstOpCode::LR, callNode, highReg, lowReg);

               TR::RegisterDependencyConditions * deps =
                  new (cg()->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg());
               deps->addPostCondition(lowReg, getLongLowReturnRegister(),DefinesDependentRegister);
               deps->addPostCondition(highReg, getLongHighReturnRegister(),DefinesDependentRegister);
               cursor->setDependencyConditions(deps);

               cg()->stopUsingRegister(lowReg);
               returnRegister = highReg;
               }
            }
         break;
      case TR::fcalli:
      case TR::dcalli:
         returnRegister = dependencies->searchPostConditionRegister(getFloatReturnRegister());
         break;
      case TR::calli:
         returnRegister = NULL;
         break;
      default:
         returnRegister = NULL;
         TR_ASSERT( 0, "Unknown indirect call Opcode.");
      }

   callNode->setRegister(returnRegister);
#if TODO // for live register - to do later
   cg()->freeAndResetTransientLongs();
#endif
   dependencies->stopUsingDepRegs(cg(), lowReg == NULL ? returnRegister : highReg, lowReg);
   return returnRegister;
   }

void
J9::Z::PrivateLinkage::setupBuildArgForLinkage(TR::Node * callNode, TR_DispatchType dispatchType, TR::RegisterDependencyConditions * deps, bool isFastJNI,
      bool isPassReceiver, int64_t & killMask, TR::Node * GlobalRegDeps, bool hasGlRegDeps, TR::SystemLinkage * systemLinkage)
   {
   TR::CodeGenerator * codeGen = cg();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   // call base class
   OMR::Z::Linkage::setupBuildArgForLinkage(callNode, dispatchType, deps, isFastJNI, isPassReceiver, killMask, GlobalRegDeps, hasGlRegDeps, systemLinkage);


   // omr todo: this should be cleaned up once the logic of other linkage related method is cleaned up
   // basically JNIDispatch will perform the stuff after this statement and hence returning here
   // to avoid executing stuff twice...should be fixed in conjunction with JNIDispatch
   if (dispatchType == TR_JNIDispatch)  return;


   J9::Z::PrivateLinkage * privateLinkage = (J9::Z::PrivateLinkage *) cg()->getLinkage(TR_Private);
   TR::RealRegister * javaStackPointerRealRegister = privateLinkage->getStackPointerRealRegister();
   TR::Register * methodMetaDataVirtualRegister = privateLinkage->getMethodMetaDataRealRegister();

   // store java stack pointer
   generateRXInstruction(codeGen, TR::InstOpCode::getStoreOpCode(), callNode, javaStackPointerRealRegister,
        new (trHeapMemory()) TR::MemoryReference(methodMetaDataVirtualRegister, (int32_t)fej9->thisThreadGetJavaSPOffset(), codeGen));

   }

void
J9::Z::PrivateLinkage::setupRegisterDepForLinkage(TR::Node * callNode, TR_DispatchType dispatchType,
   TR::RegisterDependencyConditions * &deps, int64_t & killMask, TR::SystemLinkage * systemLinkage,
   TR::Node * &GlobalRegDeps, bool &hasGlRegDeps, TR::Register ** methodAddressReg, TR::Register * &javaLitOffsetReg)
   {
   // call base class
   OMR::Z::Linkage::setupRegisterDepForLinkage(callNode, dispatchType, deps, killMask, systemLinkage, GlobalRegDeps, hasGlRegDeps, methodAddressReg, javaLitOffsetReg);


   TR::CodeGenerator * codeGen = cg();

   if (dispatchType == TR_SystemDispatch)
     {
      killMask = killAndAssignRegister(killMask, deps, methodAddressReg, (comp()->target().isLinux()) ?  TR::RealRegister::GPR14 : TR::RealRegister::GPR8 , codeGen, true);
      killMask = killAndAssignRegister(killMask, deps, &javaLitOffsetReg, (comp()->target().isLinux()) ?  TR::RealRegister::GPR8 : TR::RealRegister::GPR14 , codeGen, true);
     }

   /*****************/

   TR::RealRegister * systemStackRealRegister = systemLinkage->getStackPointerRealRegister();
   TR::Register * systemStackVirtualRegister = systemStackRealRegister;

   if (comp()->target().isZOS())
      {

      TR::RealRegister::RegNum systemStackPointerRegister;
      TR::RealRegister::RegNum systemCAAPointerRegister = ((TR::S390zOSSystemLinkage *)systemLinkage)->getCAAPointerRegister();
      TR::Register * systemCAAVirtualRegister = NULL;

      killMask = killAndAssignRegister(killMask, deps, &systemCAAVirtualRegister, systemCAAPointerRegister, codeGen, true);

      if (systemStackRealRegister->getState() != TR::RealRegister::Locked)
         {
         systemStackPointerRegister = ((TR::S390zOSSystemLinkage *)systemLinkage)->getStackPointerRegister();
         systemStackVirtualRegister = NULL;
         killMask = killAndAssignRegister(killMask, deps, &systemStackVirtualRegister, systemStackPointerRegister, codeGen, true);
         deps->addPreCondition(systemStackVirtualRegister,systemStackPointerRegister);
         }
      }

   /*****************/
   J9::Z::PrivateLinkage * privateLinkage = (J9::Z::PrivateLinkage *) cg()->getLinkage(TR_Private);


   TR::RealRegister * javaLitPoolRealRegister = privateLinkage->getLitPoolRealRegister();
   TR::Register * javaLitPoolVirtualRegister = javaLitPoolRealRegister;

   if (codeGen->isLiteralPoolOnDemandOn())
      {
      javaLitPoolVirtualRegister = NULL;
      killMask = killAndAssignRegister(killMask, deps, &javaLitPoolVirtualRegister, javaLitPoolRealRegister, codeGen, true);
      generateLoadLiteralPoolAddress(codeGen, callNode, javaLitPoolVirtualRegister);
      }


   /*****************/
   TR::Register * methodMetaDataVirtualRegister = privateLinkage->getMethodMetaDataRealRegister();


   // This logic was originally in OMR::Z::Linkage::buildNativeDispatch and the condition is cg()->supportsJITFreeSystemStackPointer().
   // The original condition is only true for J9 and only on zos, so replacing it with comp()->target().isZOS().
   if ( comp()->target().isZOS() )
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      generateRXInstruction(codeGen, TR::InstOpCode::getLoadOpCode(), callNode, systemStackVirtualRegister,
         new (trHeapMemory()) TR::MemoryReference(methodMetaDataVirtualRegister, (int32_t)fej9->thisThreadGetSystemSPOffset(), codeGen));
      }

   }


TR::RealRegister::RegNum
J9::Z::PrivateLinkage::getSystemStackPointerRegister()
   {
   return cg()->getLinkage(TR_System)->getStackPointerRegister();
   }


J9::Z::JNILinkage::JNILinkage(TR::CodeGenerator * cg, TR_LinkageConventions elc)
   :J9::Z::PrivateLinkage(cg, elc)
   {
   }
