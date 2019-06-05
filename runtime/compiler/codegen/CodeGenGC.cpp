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

//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.

#pragma csect(CODE,"TRJ9CGGCBase#C")
#pragma csect(STATIC,"TRJ9CGGCBase#S")
#pragma csect(TEST,"TRJ9CGGCBase#T")

#include "codegen/J9CodeGenerator.hpp" // IWYU pragma: keep

#include <stdint.h>
#include <string.h>
#include "env/StackMemoryRegion.hpp"
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Snippet.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/ObjectModel.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "il/symbol/ParameterSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/IGNode.hpp"
#include "infra/InterferenceGraph.hpp"
#include "infra/List.hpp"
#include "ras/Debug.hpp"

void
J9::CodeGenerator::createStackAtlas()
   {
   // Assign a GC map index to each reference parameter and each reference local.
   // Stack mapping will have to map the stack in a way that honours these indices
   //
   TR::Compilation *comp = self()->comp();
   TR::ResolvedMethodSymbol * methodSymbol = comp->getMethodSymbol();
   int32_t slotIndex = 0;

   intptrj_t stackSlotSize = TR::Compiler->om.sizeofReferenceAddress();

   // --------------------------------------------------------------------------
   // First map the parameters - the mapping of parameters is constrained by
   // the linkage, so we depend on whether the linkage maps the parameters
   // right to left or left to right.
   // We assume that the parameters are mapped contiguously
   //
   ListIterator<TR::ParameterSymbol> parameterIterator(&methodSymbol->getParameterList());

   // Find the offsets of the first and last reference parameters
   //
   int32_t sizeOfParameterAreaInBytes = methodSymbol->getNumParameterSlots() * stackSlotSize;
   int32_t firstMappedParmOffset = sizeOfParameterAreaInBytes;
   int32_t offsetInBytes;
   TR::ParameterSymbol *parmCursor;
   int32_t numberOfParmSlots;
   int32_t numberOfPaddingSlots = 0;
   void *stackMark = 0;
   int32_t *colourToGCIndexMap = 0;

   bool doLocalCompaction = (self()->getLocalsIG() && self()->getSupportsCompactedLocals()) ? true : false;

   // From here, down, any stack memory allocations will expire / die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*self()->trMemory());

   if (doLocalCompaction)
      {
      colourToGCIndexMap = (int32_t *) self()->trMemory()->allocateStackMemory(self()->getLocalsIG()->getNumberOfColoursUsedToColour() * sizeof(int32_t));
      for (int32_t i=0; i<self()->getLocalsIG()->getNumberOfColoursUsedToColour(); ++i)
         {
         colourToGCIndexMap[i] = -1;
         }
      }

   bool interpreterFrameShape = comp->getOption(TR_MimicInterpreterFrameShape);

   if (interpreterFrameShape)
      {
      firstMappedParmOffset = 0;
      numberOfParmSlots = methodSymbol->getNumParameterSlots();
      }
   else
      {
      int32_t lastMappedParmOffset = -1;
      for (parmCursor = parameterIterator.getFirst(); parmCursor; parmCursor = parameterIterator.getNext())
         {
         if ((parmCursor->isReferencedParameter() || comp->getOption(TR_FullSpeedDebug)) && parmCursor->isCollectedReference())
            {
            offsetInBytes = parmCursor->getParameterOffset();

            if (!_bodyLinkage->getRightToLeft())
               {
               offsetInBytes = sizeOfParameterAreaInBytes - offsetInBytes - stackSlotSize;
               }

            if (offsetInBytes < firstMappedParmOffset)
               {
               firstMappedParmOffset = offsetInBytes;
               }

            if (offsetInBytes > lastMappedParmOffset)
               {
               lastMappedParmOffset = offsetInBytes;
               }
            }
         }

      if (lastMappedParmOffset >= firstMappedParmOffset)
         {
         // The range of stack slots between the first and last parameter that
         // contain a collected references.
         //
         numberOfParmSlots = ((lastMappedParmOffset-firstMappedParmOffset)/stackSlotSize) + 1;
         }
      else
         {
         // No collected reference parameters.
         //
         numberOfParmSlots = 0;
         }
      }

   // Now assign GC map indices to parameters depending on the linkage mapping.
   // At the same time build the parameter map.
   //
   TR_GCStackMap * parameterMap = new (self()->trHeapMemory(), numberOfParmSlots) TR_GCStackMap(numberOfParmSlots);
   parameterIterator.reset();
   for (parmCursor = parameterIterator.getFirst(); parmCursor; parmCursor = parameterIterator.getNext())
      {
      // always has to be on stack for FSD
      if (interpreterFrameShape || comp->getOption(TR_FullSpeedDebug))
         {
         parmCursor->setParmHasToBeOnStack();
         }

      if ((parmCursor->isReferencedParameter() || interpreterFrameShape || comp->getOption(TR_FullSpeedDebug)) &&
          parmCursor->isCollectedReference())
         {
         offsetInBytes = parmCursor->getParameterOffset();
         if (!_bodyLinkage->getRightToLeft())
            {
            offsetInBytes = sizeOfParameterAreaInBytes - offsetInBytes - stackSlotSize;
            }

         // Normalize the parameter offset on the stack to a zero-based index
         // into the GC map.
         //
         slotIndex = (offsetInBytes-firstMappedParmOffset)/stackSlotSize;
         parmCursor->setGCMapIndex(slotIndex);

         if (parmCursor->getLinkageRegisterIndex()<0 ||
             parmCursor->getAllocatedIndex()<0 ||
             _bodyLinkage->hasToBeOnStack(parmCursor))
            {
            parameterMap->setBit(slotIndex);
            }
         }
      }

   // Either all the parameter slots, or just the range of parameters that contain
   // collected references.
   //
   slotIndex = numberOfParmSlots;

   // --------------------------------------------------------------------------
   // Now assign a GC map index to reference locals. When the stack is mapped,
   // these locals will have to be mapped contiguously in the stack according to
   // this index.
   //
   // Locals that need initialization during the method prologue are mapped first,
   // then the ones that do not need initialization.
   //

   bool localObjectsFound = false;
   TR_GCStackAllocMap *stackAllocMap = NULL;

   ListIterator<TR::AutomaticSymbol> automaticIterator(&methodSymbol->getAutomaticList());
   TR::AutomaticSymbol * localCursor;

   int32_t numberOfPendingPushSlots = 0;
   if (comp->getOption(TR_MimicInterpreterFrameShape))
      {
      for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
         {
         int32_t assignedIndex = localCursor->getGCMapIndex();
         if (assignedIndex >= 0)
            {
            int32_t nextIndex = assignedIndex + TR::Symbol::convertTypeToNumberOfSlots(localCursor->getDataType());
            if (nextIndex > slotIndex)
               slotIndex = nextIndex;
            }
         }

      // There can be holes at the end of auto list, where the auto was declared
      // and was never used
      //
      if (slotIndex < methodSymbol->getFirstJitTempIndex())
         {
         slotIndex = methodSymbol->getFirstJitTempIndex();
         }

      numberOfPendingPushSlots = slotIndex - methodSymbol->getFirstJitTempIndex();
      TR_ASSERT(numberOfPendingPushSlots >= 0, "assertion failure");

      for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
         {
         int32_t assignedIndex = localCursor->getGCMapIndex();
         if (assignedIndex >= 0)
            {
            // FSD requires the JIT to mimic the interpreter stack.
            //    i.e. for doubles, interpreter expects 2 full slots, even on 64bit platforms.
            // Hence, the JIT must calculate the number of slots required by interpreter and
            // not the number of address-sized slots required to store the data type.
            //
            // Note: convertTypeToNumberOfSlots does not handle aggregate types,
            // but is safe in this scenario because:
            //    1. Symbols of aggregate types have negative GC map indices.
            //    2. FSD runs at no-opt => no escape analysis => no aggregates.
            //
            localCursor->setGCMapIndex(slotIndex -
                                       assignedIndex -
                                       TR::Symbol::convertTypeToNumberOfSlots(localCursor->getDataType()) +
                                       numberOfParmSlots
                                       );
            if (debug("traceFSDStackMap"))
               {
               diagnostic("FSDStackMap: Auto moved from index %d to %d\n", assignedIndex, localCursor->getGCMapIndex());
               }
            }
         }
      }

   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      {
      if (localCursor->getGCMapIndex() < 0 &&
          localCursor->isCollectedReference() &&
          !localCursor->isLocalObject() &&
          !localCursor->isInitializedReference() &&
          !localCursor->isInternalPointer() &&
          !localCursor->isPinningArrayPointer())
         {
         if (doLocalCompaction && !localCursor->holdsMonitoredObject())
            {
            // For reference locals that share a stack slot, make sure they get
            // the same GC index.
            //
            TR_IGNode * igNode;
            if (igNode = self()->getLocalsIG()->getIGNodeForEntity(localCursor))
               {
               IGNodeColour colour = igNode->getColour();

               if (colourToGCIndexMap[colour] == -1)
                  {
                  colourToGCIndexMap[colour] = slotIndex;
                  }
               else
                  {
                  localCursor->setGCMapIndex(colourToGCIndexMap[colour]);
                  if (debug("traceCL"))
                     {
                     diagnostic("Shared GC index %d, ref local=%p, %s\n",
                                  localCursor->getGCMapIndex(), localCursor, comp->signature());
                     }

                  continue;
                  }
               }
            }

         localCursor->setGCMapIndex(slotIndex);
         slotIndex += localCursor->getNumberOfSlots();
         }
      }

   int32_t numberOfSlotsToBeInitialized = slotIndex - numberOfParmSlots;

   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      {
      if (localCursor->getGCMapIndex() < 0 &&
          localCursor->isCollectedReference() &&
          (localCursor->isLocalObject() || localCursor->isInitializedReference()) &&
          !localCursor->isInternalPointer() &&
          !localCursor->isPinningArrayPointer())
         {
         if (doLocalCompaction &&
             !localCursor->holdsMonitoredObject())
            {
            // For reference locals that share a stack slot, make sure they get
            // the same GC index.
            //
            TR_IGNode * igNode;
            if (igNode = self()->getLocalsIG()->getIGNodeForEntity(localCursor))
               {
               IGNodeColour colour = igNode->getColour();

               if (colourToGCIndexMap[colour] == -1)
                  {
                  colourToGCIndexMap[colour] = slotIndex;
                  }
               else
                  {
                  localCursor->setGCMapIndex(colourToGCIndexMap[colour]);

                  if (debug("traceCL"))
                     {
                     diagnostic("Shared GC index %d, ref local=%p, %s\n",
                                  localCursor->getGCMapIndex(), localCursor, comp->signature());
                     }

                  continue;
                  }
               }
            }


         if (localCursor->isLocalObject())
            {
               localObjectsFound = true;
               int32_t localObjectAlignment = comp->fej9()->getLocalObjectAlignmentInBytes();
               if (localObjectAlignment > stackSlotSize && (TR::Compiler->target.cpu.isX86() || TR::Compiler->target.cpu.isPower() || TR::Compiler->target.cpu.isZ()))
                  {
                  // We only get here in compressedrefs mode
                  int32_t gcMapIndexAlignment = localObjectAlignment / stackSlotSize;
                  int32_t remainder = (slotIndex - numberOfParmSlots) % gcMapIndexAlignment;
                  if (remainder)
                     {
                     slotIndex += gcMapIndexAlignment - remainder;
                     numberOfPaddingSlots += gcMapIndexAlignment - remainder;
                     traceMsg(comp, "GC index of local object %p is adjusted by +%d, and is %d now\n",localCursor, gcMapIndexAlignment - remainder, slotIndex);
                     }
                  }
            }

         localCursor->setGCMapIndex(slotIndex);
         slotIndex += localCursor->getNumberOfSlots();
         }
      }

   int32_t numberOfSlots = slotIndex;

   // --------------------------------------------------------------------------
   // Build the stack map for a method.  Start with all parameters and locals
   // being live, and selectively mark slots that are not live.
   //
   TR_GCStackMap * localMap = new (self()->trHeapMemory(), numberOfSlots) TR_GCStackMap(numberOfSlots);
   localMap->copy(parameterMap);

   // Set all the local references to be live
   //
   int32_t i;
   for (i = numberOfParmSlots; i < numberOfSlots; ++i)
      {
      localMap->setBit(i);
      }

   // Reset the bits for non-reference objects
   //
   if (comp->getOption(TR_MimicInterpreterFrameShape))
      {
      for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
         {
         if (localCursor->getGCMapIndex() >= 0 &&
             (!localCursor->isCollectedReference() ||
              localCursor->isInternalPointer() ||
             localCursor->isPinningArrayPointer()))
            {
            localMap->resetBit(localCursor->getGCMapIndex());
            if (localCursor->getSize() > stackSlotSize)
               {
               localMap->resetBit(localCursor->getGCMapIndex() + 1);
               }
            }
         }
      }

   // Reset the bits for parts of local objects that are not collected slots
   //
   if (localObjectsFound)
      {
      for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
         {
         if (localCursor->isCollectedReference() && localCursor->isLocalObject())
            {
            TR::AutomaticSymbol * localObject = localCursor->getLocalObjectSymbol();
            slotIndex = localObject->getGCMapIndex();

            if (!stackAllocMap)
               {
               stackAllocMap = new (self()->trHeapMemory(), numberOfSlots) TR_GCStackAllocMap(numberOfSlots);
               }

            stackAllocMap->setBit(slotIndex);

            int32_t * collectedSlots = localObject->getReferenceSlots();
            int32_t i = 0;

            while (*collectedSlots > 0)
               {
               int32_t collectedSlotIndex = *collectedSlots;

               // collectedSlotIndex is measured in terms of object field sizes,
               // and we want GC map slot sizes, which are not necessarily the same
               // (ie. compressed refs)
               //
               collectedSlotIndex = collectedSlotIndex * TR::Compiler->om.sizeofReferenceField() / stackSlotSize;
               for ( ; i < collectedSlotIndex; ++i)
                  {
                  localMap->resetBit(slotIndex+i);
                  }

               localMap->resetBit(slotIndex+i);
               i = collectedSlotIndex+1;
               collectedSlots++;
               }

            for ( ; i < localObject->getSize()/stackSlotSize; ++i)
               {
               localMap->resetBit(slotIndex+i);
               }
            }
         }
      }

   self()->setMethodStackMap(localMap);

   // Now create the stack atlas
   //
   TR::GCStackAtlas * atlas = new (self()->trHeapMemory()) TR::GCStackAtlas(numberOfParmSlots, numberOfSlots, self()->trMemory());
   atlas->setParmBaseOffset(firstMappedParmOffset);
   atlas->setParameterMap(parameterMap);
   atlas->setLocalMap(localMap);
   atlas->setStackAllocMap(stackAllocMap);
   atlas->setNumberOfSlotsToBeInitialized(numberOfSlotsToBeInitialized);
   atlas->setIndexOfFirstSpillTemp(numberOfSlots);
   atlas->setInternalPointerMap(0);
   atlas->setNumberOfPendingPushSlots(numberOfPendingPushSlots);
   atlas->setNumberOfPaddingSlots(numberOfPaddingSlots);
   if (comp->getOption(TR_TraceCG)) traceMsg(comp, "numberOfSlots is %d, numberOfPaddingSlots is %d\n", numberOfSlots, numberOfPaddingSlots);
   self()->setStackAtlas(atlas);

   }
