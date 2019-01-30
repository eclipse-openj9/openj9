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
#include "optimizer/FearPointAnalysis.hpp"
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

bool TR_FearPointAnalysis::virtualGuardsKillFear()
   {
   static bool kill = (feGetEnv("TR_FPAnalaysisGuardsDoNotKillFear") == NULL);
   return kill;
   }

int32_t TR_FearPointAnalysis::getNumberOfBits() { return 1; }

bool TR_FearPointAnalysis::supportsGenAndKillSets() { return true; }

TR_DataFlowAnalysis::Kind TR_FearPointAnalysis::getKind() { return FearPointAnalysis; }

TR_FearPointAnalysis *TR_FearPointAnalysis::asFearPointAnalysis() { return this; }

void TR_FearPointAnalysis::analyzeNode(TR::Node *node, vcount_t visitCount, TR_BlockStructure *block, TR_SingleBitContainer *bv) 
   {
   }

void TR_FearPointAnalysis::analyzeTreeTopsInBlockStructure(TR_BlockStructure *block)
   {
   }

/**
 * Conduct fear analysis for an set of fear generating nodes, provided 
 * as a bit vector of their global indices.
 * 
 * The default behaviour will initially search trees for children nodes generating
 * fear and then propagate this upwards. When topLevelFearOnly is enabled, this phase
 * is skipped and it is assumed that the set of fear generating nodes are all treetop nodes.
 */
TR_FearPointAnalysis::TR_FearPointAnalysis(
      TR::Compilation *comp,
      TR::Optimizer *optimizer,
      TR_Structure *rootStructure,
      TR_BitVector &fearGeneratingNodes,
      bool topLevelFearOnly,
      bool trace) :
   TR_BackwardUnionSingleBitContainerAnalysis(comp, comp->getFlowGraph(), optimizer, trace),
   _fearGeneratingNodes(fearGeneratingNodes),
   _EMPTY(comp->getNodeCount(), trMemory(), stackAlloc),
   _topLevelFearOnly(topLevelFearOnly),
   _trace(trace)
   {

   if (comp->getVisitCount() > 8000)
      comp->resetVisitCounts(1);


   // Allocate the map from node to BitVector of fear generating nodes that reach it
   // Must be before the stack mark since it will be used by the caller
   //
   _fearfulNodes = (TR_SingleBitContainer**) trMemory()->allocateStackMemory(comp->getNodeCount() * sizeof(TR_SingleBitContainer *));
   TR::NodeChecklist checklist(comp);
   for (TR::TreeTop *treeTop = comp->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      if (treeTop->getNode()->getOpCodeValue() == TR::BBStart)
         {
         TR::Block *currentBlock = treeTop->getEnclosingBlock();
         if (currentBlock->isOSRCatchBlock() || currentBlock->isOSRCodeBlock())
            {
            treeTop = currentBlock->getExit();
            continue;
            }
         }

      computeFear(comp, treeTop->getNode(), checklist);
      }

   // Only nodes in fearGeneratingNodes will have fear initially, so it is cheaper to
   // apply these directly
   //
   if (_topLevelFearOnly)
      computeFearFromBitVector(comp);


   // Allocate the block info before setting the stack mark - it will be used by
   // the caller
   //
   initializeBlockInfo();
   
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());
   performAnalysis(rootStructure, false);
   }

   }

void TR_FearPointAnalysis::computeFear(TR::Compilation *comp, TR::Node *node, TR::NodeChecklist &checklist)
   {
   if (checklist.contains(node))
      return;
   checklist.add(node);
   _fearfulNodes[node->getGlobalIndex()] = new (trStackMemory()) TR_SingleBitContainer(comp->getNodeCount(), trMemory(), stackAlloc);

   // If only the treetops are of concern then its safe to use the
   // cheaper BitVector method to compute initial fear
   //
   if (_topLevelFearOnly)
      return;

   for (int i = 0; i < node->getNumChildren(); ++i)
      {
      computeFear(comp, node->getChild(i), checklist);
      *(_fearfulNodes[node->getGlobalIndex()]) |= *(_fearfulNodes[node->getChild(i)->getGlobalIndex()]);
      }

   if (_fearGeneratingNodes.get(node->getGlobalIndex()))
      {
      if (_trace)
         traceMsg(comp, "@@ n%dn generates fear\n", node->getGlobalIndex());
      _fearfulNodes[node->getGlobalIndex()]->set();
      }
   }

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
bool TR_FearPointAnalysis::confirmFearFromBitVector(TR::Node *node)
   {
   if (_fearGeneratingNodes.get(node->getGlobalIndex()))
      return true;

   for (int i = 0; i < node->getNumChildren(); ++i)
      if (confirmFearFromBitVector(node->getChild(i)))
         return true;

   return false;
   }
#endif

void TR_FearPointAnalysis::computeFearFromBitVector(TR::Compilation *comp)
   {
   TR_BitVectorIterator nodes(_fearGeneratingNodes);
   while (nodes.hasMoreElements())
      {
      int32_t index = nodes.getNextElement();
      if (_trace)
         traceMsg(comp, "@@ n%dn generates fear\n", index);
      TR_ASSERT(_fearfulNodes[index],
         "all fear generating nodes must be treetop nodes when using topLevelFearOnly, otherwise the data structure may not be initialized");
      _fearfulNodes[index]->set();
      }

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   // Do a complete pass to confirm the cheaper approach was used correctly
   TR::NodeChecklist checklist(comp);
   for (TR::TreeTop *treeTop = comp->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      if (treeTop->getNode()->getOpCodeValue() == TR::BBStart)
         {
         TR::Block *currentBlock = treeTop->getEnclosingBlock();
         if (currentBlock->isOSRCatchBlock() || currentBlock->isOSRCodeBlock())
            {
            treeTop = currentBlock->getExit();
            continue;
            }
         }

      bool fearful = confirmFearFromBitVector(treeTop->getNode());
      TR_ASSERT(_fearfulNodes[treeTop->getNode()->getGlobalIndex()]->get() == fearful,
         "all fear generating nodes must be treetop nodes when using topLevelFearOnly, otherwise the initial fear may be incorrect");
      }
#endif
   }

TR_SingleBitContainer *TR_FearPointAnalysis::generatedFear(TR::Node *node)
   {
   TR_SingleBitContainer *returnValue = _fearfulNodes[node->getGlobalIndex()];
   if (!returnValue)
     return &_EMPTY; 
   return returnValue;
   }

void TR_FearPointAnalysis::initializeGenAndKillSetInfo()
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
   for (TR::TreeTop *treeTop = comp()->findLastTree(); treeTop; treeTop = treeTop->getPrevTreeTop())
      {
      if (treeTop->getNode()->getOpCodeValue() == TR::BBEnd)
         {
         exceptingTTSeen = false;
         currentBlock = treeTop->getEnclosingBlock();
         if (currentBlock->isOSRCatchBlock() || currentBlock->isOSRCodeBlock())
            {
            _regularKillSetInfo[currentBlock->getNumber()]->setAll(getNumberOfBits());
            _exceptionKillSetInfo[currentBlock->getNumber()]->setAll(getNumberOfBits());
            treeTop = currentBlock->getEntry();
            }
         continue;
         }

      if (treeTop->getNode()->getOpCode().canRaiseException())
         {
         exceptingTTSeen = true;
         _exceptionKillSetInfo[currentBlock->getNumber()]->empty();
         }

      if (comp()->isPotentialOSRPointWithSupport(treeTop))
         {
         _regularKillSetInfo[currentBlock->getNumber()]->setAll(getNumberOfBits());
         _exceptionKillSetInfo[currentBlock->getNumber()]->setAll(getNumberOfBits());
         _regularGenSetInfo[currentBlock->getNumber()]->empty();
         }

      // kill any fear originating from inside
      if (virtualGuardsKillFear()
          && treeTop->getNode()->isTheVirtualGuardForAGuardedInlinedCall()
          && comp()->cg()->supportsMergingGuards())
         {
         _regularKillSetInfo[currentBlock->getNumber()]->setAll(getNumberOfBits());
         _exceptionKillSetInfo[currentBlock->getNumber()]->setAll(getNumberOfBits());
         //*(_regularKillSetInfo[currentBlock->getNumber()]) |= *(_inlinedCalleeMasks[treeTop->getNode()->getByteCodeInfo().getCallerIndex()]);
         //*(_exceptionKillSetInfo[currentBlock->getNumber()]) |= *(_inlinedCalleeMasks[treeTop->getNode()->getByteCodeInfo().getCallerIndex()]);
         }

      TR_SingleBitContainer *fear = generatedFear(treeTop->getNode());
      *(_regularGenSetInfo[currentBlock->getNumber()] ) |= *fear;
      if (exceptingTTSeen)
         *(_exceptionGenSetInfo[currentBlock->getNumber()]) |= *fear;
      }
   }

bool TR_FearPointAnalysis::postInitializationProcessing()
   {
   return true;
   }

