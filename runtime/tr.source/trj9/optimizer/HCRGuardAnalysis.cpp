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
#include <stddef.h>                        // for NULL
#include <stdint.h>                        // for int32_t
#include "optimizer/HCRGuardAnalysis.hpp"
#include "env/StackMemoryRegion.hpp"
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"         // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"                // for TR_Memory, etc
#include "il/Node.hpp"                     // for Node, vcount_t
#include "il/Node_inlines.hpp"
#include "infra/Assert.hpp"                // for TR_ASSERT
#include "infra/BitVector.hpp"             // for TR_BitVector
#include "infra/Checklist.hpp"             // for TR::NodeChecklist

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
   for (int32_t i = 0; i < comp()->getFlowGraph()->getNextNodeNumber(); ++i)
      {
      _regularGenSetInfo[i] = new (trStackMemory()) TR_SingleBitContainer(getNumberOfBits(),trMemory(), stackAlloc);
      _exceptionGenSetInfo[i] = new (trStackMemory()) TR_SingleBitContainer(getNumberOfBits(),trMemory(), stackAlloc);
      _regularKillSetInfo[i] = new (trStackMemory()) TR_SingleBitContainer(getNumberOfBits(),trMemory(), stackAlloc);
      _exceptionKillSetInfo[i] = new (trStackMemory()) TR_SingleBitContainer(getNumberOfBits(),trMemory(), stackAlloc);
      }

   TR::Block *currentBlock = NULL;
   bool exceptingTTSeen = false;
   TR::NodeChecklist checklist(comp());
   TR::Node *osrNode;

   TR_ByteCodeInfo nodeBCI;
   nodeBCI.setCallerIndex(-1);
   nodeBCI.setByteCodeIndex(0);
   nodeBCI.setDoNotProfile(false);
   currentBlock = comp()->getStartTree()->getEnclosingBlock();
   if (!comp()->getMethodSymbol()->supportsInduceOSR(nodeBCI, currentBlock, comp()))
      _regularGenSetInfo[currentBlock->getNumber()]->setAll(getNumberOfBits());

   for (TR::TreeTop *treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      if (treeTop->getNode()->getOpCodeValue() == TR::BBStart)
         {
         exceptingTTSeen = false;
         currentBlock = treeTop->getEnclosingBlock();
         if (shouldSkipBlock(currentBlock))
            {
            _regularKillSetInfo[currentBlock->getNumber()]->setAll(getNumberOfBits());
            _exceptionKillSetInfo[currentBlock->getNumber()]->setAll(getNumberOfBits());
            treeTop = currentBlock->getExit();
            }
         continue;
         }

      if (treeTop->getNode()->getOpCode().canRaiseException())
         {
         exceptingTTSeen = true;
         }

      osrNode = NULL;
      if (comp()->isPotentialOSRPoint(treeTop->getNode(), &osrNode) && !checklist.contains(osrNode))
         {
         checklist.add(osrNode);
         bool supportsOSR = comp()->isPotentialOSRPointWithSupport(treeTop);
         if (supportsOSR)
            {
            _regularKillSetInfo[currentBlock->getNumber()]->setAll(getNumberOfBits());
            if (exceptingTTSeen)
               _exceptionKillSetInfo[currentBlock->getNumber()]->setAll(getNumberOfBits());
            }
         /*else
            {
            TR::Node *node = treeTop->getNode();
            traceMsg(comp(), "mismatch at n%dn %d:%d %s\n", node->getGlobalIndex(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex(),  comp()->signature());
            }*/
         if (!supportsOSR && treeTop->getNode()->getByteCodeInfo().getCallerIndex() > -1)
            {
            _regularGenSetInfo[currentBlock->getNumber()]->setAll(getNumberOfBits());
            if (exceptingTTSeen)
               *(_exceptionGenSetInfo[currentBlock->getNumber()]) |= *(_regularGenSetInfo[currentBlock->getNumber()]);
            }
         }
      else if (treeTop->getNode()->isTheVirtualGuardForAGuardedInlinedCall() && comp()->cg()->supportsMergingGuards())
         {
         TR_VirtualGuard *guardInfo = comp()->findVirtualGuardInfo(treeTop->getNode());
         if (guardInfo->getKind() != TR_HCRGuard)
            {
            _regularKillSetInfo[currentBlock->getNumber()]->setAll(getNumberOfBits());
            if (exceptingTTSeen)
               _exceptionKillSetInfo[currentBlock->getNumber()]->setAll(getNumberOfBits());
            }
         }
      }
   }

bool TR_HCRGuardAnalysis::postInitializationProcessing()
   {
   return true;
   }
