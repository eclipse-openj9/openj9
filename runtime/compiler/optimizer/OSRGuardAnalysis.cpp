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
#include "optimizer/OSRGuardAnalysis.hpp"

#include "codegen/CodeGenerator.hpp"
#include "env/StackMemoryRegion.hpp"
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "infra/BitVector.hpp"

/**
 * This data flow analysis will detect OSR guards that are no longer required. This is done by generating at yields
 * to the VM and killing at OSR guards. Any OSR guards without a yield that reaches them can be removed.
 *
 * It will perform the analysis and cache any blocks that contain yields within them.
 */
TR_OSRGuardAnalysis::TR_OSRGuardAnalysis(TR::Compilation *comp, TR::Optimizer *optimizer, TR_Structure *rootStructure) :
   TR_UnionSingleBitContainerAnalysis(comp, comp->getFlowGraph(), optimizer, false)
   {
   if (comp->getVisitCount() > 8000)
      comp->resetVisitCounts(1);

   _containsYields = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc, growable);
   initializeBlockInfo();
   
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());
   performAnalysis(rootStructure, false);
   }

   }

int32_t TR_OSRGuardAnalysis::getNumberOfBits() { return 1; }

bool TR_OSRGuardAnalysis::supportsGenAndKillSets() { return true; }

TR_DataFlowAnalysis::Kind TR_OSRGuardAnalysis::getKind() { return OSRGuardAnalysis; }

void TR_OSRGuardAnalysis::analyzeNode(TR::Node *node, vcount_t visitCount, TR_BlockStructure *block, TR_SingleBitContainer *bv) 
   {
   }

void TR_OSRGuardAnalysis::analyzeTreeTopsInBlockStructure(TR_BlockStructure *block)
   {
   }

bool TR_OSRGuardAnalysis::shouldSkipBlock(TR::Block *block)
   {
   return block->isOSRCatchBlock() || block->isOSRCodeBlock() || block->isOSRInduceBlock();
   }

bool TR_OSRGuardAnalysis::containsYields(TR::Block *block)
   {
   return _containsYields->get(block->getNumber());
   }

bool TR_OSRGuardAnalysis::postInitializationProcessing()
   {
   return true;
   }

void TR_OSRGuardAnalysis::initializeGenAndKillSetInfo()
   {
   for (int32_t i = 0; i < comp()->getFlowGraph()->getNextNodeNumber(); ++i)
      {
      _regularGenSetInfo[i] = new (trStackMemory()) TR_SingleBitContainer(getNumberOfBits(),trMemory(), stackAlloc);
      _exceptionGenSetInfo[i] = new (trStackMemory()) TR_SingleBitContainer(getNumberOfBits(),trMemory(), stackAlloc);
      _regularKillSetInfo[i] = new (trStackMemory()) TR_SingleBitContainer(getNumberOfBits(),trMemory(), stackAlloc);
      _exceptionKillSetInfo[i] = new (trStackMemory()) TR_SingleBitContainer(getNumberOfBits(),trMemory(), stackAlloc);
      }

   TR::Block *block = comp()->getStartTree()->getEnclosingBlock();
   // The method entry generates fear, so set the first block as generating
   _regularGenSetInfo[block->getNumber()]->setAll(getNumberOfBits());
   _containsYields->set(block->getNumber());

   for (TR::TreeTop *treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      if (treeTop->getNode()->getOpCodeValue() == TR::BBStart)
         {
         block = treeTop->getEnclosingBlock();
         if (shouldSkipBlock(block))
            {
            _regularKillSetInfo[block->getNumber()]->setAll(getNumberOfBits());
            _exceptionKillSetInfo[block->getNumber()]->setAll(getNumberOfBits());
            treeTop = block->getExit();
            }
         continue;
         }

      // An exception treetop could result in an edge to a catch block, which should
      // require protection based on the current state of this block
      if (treeTop->getNode()->getOpCode().canRaiseException())
         {
         *(_exceptionGenSetInfo[block->getNumber()]) |= *(_regularGenSetInfo[block->getNumber()]);
         }

      // Identify yield points, treetops that must be protected by OSR guards
      if (comp()->isPotentialOSRPoint(treeTop->getNode()))
         {
         _regularGenSetInfo[block->getNumber()]->setAll(getNumberOfBits());
         _containsYields->set(block->getNumber());
         }

      // Identify OSR guards, treetops that implement the protection
      else if (treeTop->getNode()->isOSRGuard())
         {
         _regularKillSetInfo[block->getNumber()]->setAll(getNumberOfBits());
         _regularGenSetInfo[block->getNumber()]->empty();
         if (block->isCatchBlock())
            _exceptionKillSetInfo[block->getNumber()]->setAll(getNumberOfBits());
         }

      // Identify VG merged with OSR guards
      else if (treeTop->getNode()->isTheVirtualGuardForAGuardedInlinedCall() && comp()->cg()->supportsMergingGuards())
         {
         TR_VirtualGuard *guardInfo = comp()->findVirtualGuardInfo(treeTop->getNode());
         if (guardInfo->mergedWithOSRGuard())
            {
            _regularKillSetInfo[block->getNumber()]->setAll(getNumberOfBits());
            _regularGenSetInfo[block->getNumber()]->empty();
            if (block->isCatchBlock())
               _exceptionKillSetInfo[block->getNumber()]->setAll(getNumberOfBits());
            }
         }
      }
   }
