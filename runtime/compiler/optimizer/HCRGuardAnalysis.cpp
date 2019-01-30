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
#include <stddef.h>
#include <stdint.h>
#include "optimizer/HCRGuardAnalysis.hpp"
#include "env/StackMemoryRegion.hpp"
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Checklist.hpp"
#include "optimizer/FearPointAnalysis.hpp"

static bool containsPrepareForOSR(TR::Block *block)
   {
   for (TR::TreeTop *tt = block->getEntry(); tt != block->getExit(); tt = tt->getNextTreeTop())
       {
       if (tt->getNode()->getOpCode().isCheck() || tt->getNode()->getOpCodeValue() == TR::treetop)
          {
          if (tt->getNode()->getFirstChild()->getOpCode().isCall()
              && tt->getNode()->getFirstChild()->getSymbolReference()->getReferenceNumber() == TR_prepareForOSR)
             return true;
          }
       }
   return false;
   }

int32_t TR_HCRGuardAnalysis::getNumberOfBits() { return 1; }

bool TR_HCRGuardAnalysis::supportsGenAndKillSets() { return true; }

TR_DataFlowAnalysis::Kind TR_HCRGuardAnalysis::getKind() { return HCRGuardAnalysis; }

void TR_HCRGuardAnalysis::analyzeNode(TR::Node *node, vcount_t visitCount, TR_BlockStructure *block, TR_SingleBitContainer *bv) 
   {
   }

void TR_HCRGuardAnalysis::analyzeTreeTopsInBlockStructure(TR_BlockStructure *block)
   {
   }

TR_HCRGuardAnalysis::TR_HCRGuardAnalysis(TR::Compilation *comp, TR::Optimizer *optimizer, TR_Structure *rootStructure) :
   TR_UnionSingleBitContainerAnalysis(comp, comp->getFlowGraph(), optimizer, false/*comp->getOption(TR_TraceFearPointAnalysis)*/)
   {
   //_traceFearPointAnalysis = comp->getOption(TR_TraceFearPointAnalysis);
   if (comp->getVisitCount() > 8000)
      comp->resetVisitCounts(1);

   // Allocate the block info before setting the stack mark - it will be used by
   // the caller
   //
   initializeBlockInfo();
   
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());
   performAnalysis(rootStructure, false);
   }

   }

bool TR_HCRGuardAnalysis::shouldSkipBlock(TR::Block *block)
   {
   return block->isOSRCatchBlock() || block->isOSRCodeBlock() || containsPrepareForOSR(block);
   }

void TR_HCRGuardAnalysis::initializeGenAndKillSetInfo()
   {
   int32_t numBits = getNumberOfBits();
   for (int32_t i = 0; i < comp()->getFlowGraph()->getNextNodeNumber(); ++i)
      {
      _regularGenSetInfo[i] = new (trStackMemory()) TR_SingleBitContainer(numBits,trMemory(), stackAlloc);
      _exceptionGenSetInfo[i] = new (trStackMemory()) TR_SingleBitContainer(numBits,trMemory(), stackAlloc);
      _regularKillSetInfo[i] = new (trStackMemory()) TR_SingleBitContainer(numBits,trMemory(), stackAlloc);
      _exceptionKillSetInfo[i] = new (trStackMemory()) TR_SingleBitContainer(numBits,trMemory(), stackAlloc);
      }

   TR::Block *currentBlock = NULL;
   int32_t blockId = -1;
   TR::NodeChecklist checklist(comp());
   TR::NodeChecklist exceptionNodelist(comp());
   bool isGen;            /* If current state is gen */
   bool isKill;           /* If current state is kill */
   bool hasExceptionGen;  /* If there exists an exception that gens*/
   bool hasExceptionKill;  /* If there exists an exception that kills*/
   bool hasExceptionNotKill;  /* If there exists an exception that doesn't kill*/
   bool isYieldPoint;

   TR_ByteCodeInfo nodeBCI;
   nodeBCI.setCallerIndex(-1);
   nodeBCI.setByteCodeIndex(0);
   nodeBCI.setDoNotProfile(false);
   currentBlock = comp()->getStartTree()->getEnclosingBlock();
   if (!comp()->getMethodSymbol()->supportsInduceOSR(nodeBCI, currentBlock, comp()))
      _regularGenSetInfo[currentBlock->getNumber()]->setAll(numBits);

   for (TR::TreeTop *treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node* ttNode = treeTop->getNode();
      TR::Node* osrNode = NULL;
      isYieldPoint = false;
      if (ttNode->getOpCodeValue() == TR::BBStart)
         {
         currentBlock = treeTop->getEnclosingBlock();
         blockId = currentBlock->getNumber();
         // Reset the walk state at the beginning of each block
         isGen = false;
         isKill = false;
         hasExceptionGen = false;
         hasExceptionKill = false;
         hasExceptionNotKill = false;
         if (shouldSkipBlock(currentBlock))
            {
            _regularKillSetInfo[blockId]->setAll(numBits);
            _regularGenSetInfo[blockId]->empty();
            _exceptionKillSetInfo[blockId]->setAll(numBits);
            _exceptionGenSetInfo[blockId]->empty();

            treeTop = currentBlock->getExit();
            }
         continue;
         }
      else if (ttNode->getOpCodeValue() == TR::BBEnd)
         {
         // Gen and kill are exclusive, so isGen and isKill cannot be true at the same time
         if (isGen)
            {
            TR_ASSERT(!isKill, "isGen and isKill cannot be true at the same time");
            _regularGenSetInfo[blockId]->setAll(numBits);
            _regularKillSetInfo[blockId]->empty();
            }
         else if (isKill)
            {
            TR_ASSERT(!isGen, "isGen and isKill cannot be true at the same time");
            _regularKillSetInfo[blockId]->setAll(numBits);
            _regularGenSetInfo[blockId]->empty();
            }

         // Gen should win if there exists one exception point that is or can be reached by gen
         if (hasExceptionGen)
            {
            _exceptionGenSetInfo[blockId]->setAll(numBits);
            _exceptionKillSetInfo[blockId]->empty();
            }
         else if (hasExceptionKill && !hasExceptionNotKill) /* All exception points have to be kill in order to kill along the exception path */
            {
            _exceptionKillSetInfo[blockId]->setAll(numBits);
            _exceptionGenSetInfo[blockId]->empty();
            }

         continue;
         }

      if (comp()->isPotentialOSRPoint(ttNode, &osrNode) && !checklist.contains(osrNode))
         {
         checklist.add(osrNode);
         isYieldPoint = true;
         bool supportsOSR = comp()->isPotentialOSRPointWithSupport(treeTop);
         if (supportsOSR)
            {
            isKill = true;
            isGen = false;
            }
         else
            {
            isGen = true;
            isKill = false;
            }
         }
      else if (ttNode->isTheVirtualGuardForAGuardedInlinedCall()
               && TR_FearPointAnalysis::virtualGuardsKillFear()
               && comp()->cg()->supportsMergingGuards())
         {
         TR_VirtualGuard *guardInfo = comp()->findVirtualGuardInfo(ttNode);
         if (guardInfo->getKind() != TR_HCRGuard)
            {
            // Theoretically, the guard should only kill its inlined path. However, making it right require adding
            // complications to the data flow and/or optimizations using the result of the analysis. Based on the
            // fact that there is few optimization opportunities on the taken side and optimizing it has little benefit,
            // we require optimizations that can generate fear stay away from taken side such that considering a guard
            // to be a kill for the taken side is safe.
            //
            isKill = true;
            isGen = false;
            }
         }

      bool canNodeRaiseException = false;
      if (ttNode->getOpCode().canRaiseException())
         {
         canNodeRaiseException = true;
         }
      else
         {
         // Non-treetop nodes that can raise exceptions
         if (ttNode->getOpCodeValue() == TR::treetop && ttNode->getNumChildren() > 0)
            {
            TR::Node* node = ttNode->getFirstChild();
            if (!exceptionNodelist.contains(node) && node->getOpCode().canRaiseException())
               {
               canNodeRaiseException = true;
               exceptionNodelist.add(node);
               }
            }
         }

      if (canNodeRaiseException)
         {
         // If the exception point is also a yield point, it has to be a gen.
         // It's possible for the tree to yield, allowing assumptions to be invalidated,
         // and then throw afterward. An OSR guard (if one is necessary) would only run
         // after non-exceptional completion of the tree, so it wouldn't stop control from
         // reaching the exception handler.
         if (isYieldPoint)
            {
            hasExceptionGen = true;
            }
         else
            {
            hasExceptionGen = hasExceptionGen || isGen;
            hasExceptionKill = hasExceptionKill || isKill;
            hasExceptionNotKill = hasExceptionNotKill || !isKill;
            }
         }

      }
   }

bool TR_HCRGuardAnalysis::postInitializationProcessing()
   {
   return true;
   }
