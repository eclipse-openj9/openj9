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
#include "OSRGuardInsertion.hpp"

#include "codegen/CodeGenerator.hpp"
#include "compile/OSRData.hpp"
#include "compile/VirtualGuard.hpp"
#include "il/Block.hpp"
#include "infra/Assert.hpp"
#include "infra/ILWalk.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/FearPointAnalysis.hpp"
#include "optimizer/RematTools.hpp"
#include "optimizer/TransformUtil.hpp"
#include "ras/DebugCounter.hpp"
#include "infra/Checklist.hpp"

TR_Structure* fakeRegion(TR::Compilation *comp);

static bool generatesFear(TR::Compilation *comp, TR_FearPointAnalysis &fearAnalysis, TR::Block *block)
   {
   return fearAnalysis._blockAnalysisInfo[block->getNumber()]->get(0);
   }


static bool hasHCRGuard(TR::Compilation *comp)
   {
   TR::list<TR_VirtualGuard*> &virtualGuards = comp->getVirtualGuards();
   for (auto itr = virtualGuards.begin(), end = virtualGuards.end(); itr != end; ++itr)
      {
      if ((*itr)->getKind() == TR_HCRGuard || (*itr)->mergedWithHCRGuard())
         return true;
      }

   return false;
   }

static bool hasOSRFearPoint(TR::Compilation *comp)
   {
   for (TR::TreeTop *treeTop = comp->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *ttNode = treeTop->getNode();
      if (ttNode->getNumChildren() == 1 &&
          ttNode->getFirstChild()->isOSRFearPointHelperCall())
         {
         return true;
         }
      }

   return false;
   }

static bool hasUnsupportedPotentialOSRPoint(TR::Compilation *comp, bool trace)
   {
   for (TR::TreeTop *treeTop = comp->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *ttNode = treeTop->getNode();

      if (comp->isPotentialOSRPoint(ttNode) &&
          !comp->isPotentialOSRPointWithSupport(treeTop))
         {
         if (trace)
            traceMsg(comp, "Found an unsupported potential OSR point at n%dn\n", ttNode->getGlobalIndex());
         return true;
         }
      }

   return false;
   }

void TR_OSRGuardInsertion::cleanUpPotentialOSRPointHelperCalls()
   {
   for (TR::TreeTop *treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *ttNode = treeTop->getNode();
      if (ttNode->getNumChildren() != 1)
         continue;

      TR::Node *node = ttNode->getFirstChild();
      if (node->isPotentialOSRPointHelperCall())
         {
         TR::TreeTop* prevTreeTop = treeTop->getPrevTreeTop();
         TR::TransformUtil::removeTree(comp(), treeTop);
         treeTop = prevTreeTop;
         }
      }
   }


/** \brief
 *     Remove redundant potentialOSRPointHelper calls using HCRGuardAnalysis.
 *
 *     A potentialOSRPointHelper call not reached by a gen is considered redundant.
 *     The effect of a potentialOSRPointHelper call in data flow is to kill. If no
 *     gen reaches it, it doesn't matter if we kill there. Thus removing the redundant
 *     potentialOSRPoint helper calls will not change the result of HCRGuardAnalysis if
 *     we were to repeat it after the removal.
 *
 *  \param guardAnalysis
 *     Result of HCRGuardAnalysis used to determine which helper call is redundant.
 *
 */
void TR_OSRGuardInsertion::removeRedundantPotentialOSRPointHelperCalls(TR_HCRGuardAnalysis* guardAnalysis)
   {
   bool protectedByOSRPoints = false;
   TR::NodeChecklist visited(comp());

   for (TR::TreeTop *treeTop = comp()->getStartTree();
        treeTop;
        treeTop = treeTop->getNextRealTreeTop())
      {
      TR::Node *ttNode = treeTop->getNode();

      if (ttNode->getOpCodeValue() == TR::BBStart)
         {
         TR::Block* block = treeTop->getEnclosingBlock();
         protectedByOSRPoints = !guardAnalysis || guardAnalysis->_blockAnalysisInfo[block->getNumber()]->isEmpty();
         continue;
         }

      TR::Node *osrNode = NULL;
      if (comp()->isPotentialOSRPoint(ttNode, &osrNode))
         {
         if (visited.contains(osrNode))
            continue;

         if (protectedByOSRPoints &&
             osrNode->isPotentialOSRPointHelperCall())
            {
            dumpOptDetails(comp(), "Remove redundant potentialOSRPointHelper call n%dn %p\n", osrNode->getGlobalIndex(), osrNode);

            TR::TreeTop* prevTree = treeTop->getPrevTreeTop();
            TR::TransformUtil::removeTree(comp(), treeTop);
            treeTop = prevTree;
            }
         else if (comp()->isPotentialOSRPointWithSupport(treeTop))
            {
            if (!protectedByOSRPoints && trace())
               traceMsg(comp(), "treetop n%dn is an OSR point with support\n", ttNode->getGlobalIndex());

            protectedByOSRPoints = true;
            }
         else
            {
            if (protectedByOSRPoints && trace())
               traceMsg(comp(), "treetop n%dn is an OSR point without support\n", ttNode->getGlobalIndex());

            protectedByOSRPoints = false;
            }

         visited.add(osrNode);
         continue;
         }
      }

   if (trace())
      {
      comp()->dumpMethodTrees("Trees after redundant potentialOSRPointHelper call removal", comp()->getMethodSymbol());
      }
   }

static bool skipOSRGuardInsertion(TR::Compilation* comp)
   {
   // Use with caution. There can be fear points generated before OSRGuardInsertion,
   // disabling it will leave those fear points unprotected.
   //
   static char *disableOSRGuards = feGetEnv("TR_DisableOSRGuards");

   // Currently, OSR guard insertion is only needed when in the OSR HCR mode
   if (disableOSRGuards || !comp->isOSRTransitionTarget(TR::postExecutionOSR))
      {
      return true;
      }

   // Even under NextGenHCR, OSR may have been disabled for this compilation at runtime
   if (!comp->supportsInduceOSR())
      {
      return true;
      }

   return false;
   }

int32_t TR_OSRGuardInsertion::perform()
   {
   bool needHCRGuardRemoval = hasHCRGuard(comp());
   bool hasFearPoint = hasOSRFearPoint(comp());
   bool canInsertOSRGuards = !skipOSRGuardInsertion(comp());

   TR_ASSERT_FATAL(!hasFearPoint || canInsertOSRGuards, "Fear point exists without OSR protection");

   if (canInsertOSRGuards && (needHCRGuardRemoval || hasFearPoint))
      {
      bool hasPotentialOSRPointWithoutSupport = hasUnsupportedPotentialOSRPoint(comp(), trace());
      bool requiresAnalysis = hasPotentialOSRPointWithoutSupport;

      static char *disableHCRGuardAnalysis = feGetEnv("TR_DisableHCRGuardAnalysis");
      if (disableHCRGuardAnalysis != NULL)
         requiresAnalysis = false;

      if (requiresAnalysis)
         TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "requiresAnalysis/(%s %s)", comp()->signature(), comp()->getHotnessName(comp()->getMethodHotness())));

      // Create the fake region
      //
      TR_Structure *structure = requiresAnalysis ? fakeRegion(comp()) : NULL;
      comp()->getFlowGraph()->setStructure(structure);

      TR_HCRGuardAnalysis *guardAnalysis = requiresAnalysis ? new (comp()->allocator()) TR_HCRGuardAnalysis(comp(), optimizer(), structure) : NULL;
      TR_BitVector fearGeneratingNodes(comp()->getNodeCount(), trMemory(), stackAlloc);

      if (hasPotentialOSRPointWithoutSupport)
         {
         // Redudant potentialOSRPointHelper calls may result in more OSR guards than needed
         //
         removeRedundantPotentialOSRPointHelperCalls(guardAnalysis);
         }
      else
         {
         // There is no gen, all helper calls are redundant.
         //
         cleanUpPotentialOSRPointHelperCalls();
         }

      // Handle fear nodes for HCR guards, branch to slow path might be removed
      //
      if (needHCRGuardRemoval)
         removeHCRGuards(fearGeneratingNodes, guardAnalysis);

      if (hasFearPoint)
         collectFearFromOSRFearPointHelperCalls(fearGeneratingNodes, guardAnalysis);

      // Future fear generating optimizations
      //
      if (!fearGeneratingNodes.isEmpty())
         {
         insertOSRGuards(fearGeneratingNodes);
         }
      else
         {
         if (trace())
            traceMsg(comp(), "No fear generating nodes - skipping\n");
         comp()->getFlowGraph()->invalidateStructure();
         }
      }

   // Must be done
   //
   cleanUpPotentialOSRPointHelperCalls();
   cleanUpOSRFearPoints();

   return 0;
   }

const char *
TR_OSRGuardInsertion::optDetailString() const throw()
   {
   return "O^O OSR GUARD INS: ";
   }

/*
 * Generate a region which contains all blocks to serve as the structure in the upcoming data flows.
 * This reduces the compile time overhead, as the structural analysis won't improve analysis times for
 * fear analysis and HCR guard removal due to the kills present in loops for asyncchecks.
 */
TR_Structure* fakeRegion(TR::Compilation *comp)
   {
   TR::CFG* cfg = comp->getFlowGraph();
   // This is the memory region into which we allocate structure nodes
   TR::Region &structureRegion = cfg->structureRegion();
   TR::CFGNode *cfgNode;

   TR_Array<TR_StructureSubGraphNode*> *blocks =  new (comp->trStackMemory()) TR_Array<TR_StructureSubGraphNode*>(comp->trMemory(), cfg->getNumberOfNodes(), false, stackAlloc);

   // This region is the CFG node grouping we are building to facilitate dataflow analysis
   // it is allocated into the memory region for structure nodes
   TR_RegionStructure *region = new (structureRegion) TR_RegionStructure(comp, 0);
   for (cfgNode = cfg->getFirstNode(); cfgNode; cfgNode = cfgNode->getNext())
      {
      TR::Block *block = toBlock(cfgNode);
      (*blocks)[block->getNumber()] = new (structureRegion) TR_StructureSubGraphNode(new (structureRegion) TR_BlockStructure(comp, block->getNumber(), block));
      region->addSubNode((*blocks)[block->getNumber()]);
      }

   for (cfgNode = cfg->getFirstNode(); cfgNode; cfgNode = cfgNode->getNext())
      {
      TR::Block *b = toBlock(cfgNode);
      TR::Block *next;

      for (auto p = b->getSuccessors().begin(); p != b->getSuccessors().end(); ++p)
         {
         next = toBlock((*p)->getTo());
         TR::CFGEdge::createEdge((*blocks)[b->getNumber()], (*blocks)[next->getNumber()], comp->trMemory());
         }
      for (auto p = b->getExceptionSuccessors().begin(); p != b->getExceptionSuccessors().end(); ++p)
         {
         next = toBlock((*p)->getTo());
         TR::CFGEdge::createExceptionEdge((*blocks)[b->getNumber()], (*blocks)[next->getNumber()], comp->trMemory());
         }
      }

   region->setContainsImproperRegion(true);
   if (comp->mayHaveLoops())
      region->setContainsInternalCycles(true);
   region->setEntry((*blocks)[0]);
   return region;
   }


void TR_OSRGuardInsertion::removeHCRGuards(TR_BitVector &fearGeneratingNodes, TR_HCRGuardAnalysis* guardAnalysis)
   {
   for (TR::Block *cursor = comp()->getStartBlock(); cursor != NULL; cursor = cursor->getNextBlock())
      {
      TR::TreeTop *lastTree = cursor->getLastRealTreeTop();
      if (!lastTree) { continue; }
      TR::Node *node = lastTree->getNode();

      if (!node->isTheVirtualGuardForAGuardedInlinedCall()) { continue; }
      TR_VirtualGuard *guardInfo = comp()->findVirtualGuardInfo(node);
      TR_ASSERT(guardInfo, "we expect to get virtual guard info in HCRGuardRemoval!");
      TR_ASSERT(!guardInfo->getByteCodeInfo().isInvalidByteCodeIndex(), "we expect to get valid bytecode info for a virtual guard!");

      if (!guardAnalysis || guardAnalysis->_blockAnalysisInfo[cursor->getNextBlock()->getNumber()]->isEmpty())
         {
         if (guardInfo->getKind() == TR_HCRGuard
             && performTransformation(comp(), "O^O HCR GUARD REMOVAL: removing HCRGuard node n%dn\n", node->getGlobalIndex()))
            {
            comp()->addClassForOSRRedefinition(guardInfo->getThisClass());
            comp()->removeVirtualGuard(guardInfo);

            // When removing the HCR branch, first remove the successor edges from the existing HCR block
            // This allows the branch removal operation to skip expensive checks when structure has been modified
            //
            TR::Block *takenBlock = node->getBranchDestination()->getEnclosingBlock();
            if (takenBlock->getPredecessors().size() == 1)
               {
               TR_ASSERT(takenBlock->getSuccessors().size() == 1, "HCR block should have exactly one successor");
               comp()->getFlowGraph()->removeEdge(takenBlock->getSuccessors().front());

               while (takenBlock->getExceptionSuccessors().size() > 0)
                  comp()->getFlowGraph()->removeEdge(takenBlock->getExceptionSuccessors().front());
               }

            cursor->removeBranch(comp());

            // Check whether there is another virtual guard, with the same branch destination
            // Based on the inliner, this guard should be in the prior block and there should
            // be no other predecessors
            //
            TR::Node *potentialGuard = NULL;
            bool additionalVirtualGuard = false;
            if (cursor->getPredecessors().size() == 1
                && cursor->getPredecessors().front()->getFrom()->asBlock() != comp()->getFlowGraph()->getStart())
               {
               potentialGuard = cursor->getPredecessors().front()->getFrom()->asBlock()->getLastRealTreeTop()->getNode();
               additionalVirtualGuard = potentialGuard->isTheVirtualGuardForAGuardedInlinedCall()
                  && potentialGuard->getBranchDestination()->getEnclosingBlock() == takenBlock;
               }

            // if virtual guards kill fear we can short cut additional analysis if the method still has a guard
            // we can mark that guard as an OSR guard and continue without needing a data flow analysis
            if (TR_FearPointAnalysis::virtualGuardsKillFear()
                && additionalVirtualGuard
                && comp()->cg()->supportsMergingGuards())
               {
               TR_VirtualGuard *additionalGuardInfo = comp()->findVirtualGuardInfo(potentialGuard);
               TR_ASSERT(additionalGuardInfo, "guard info should exist for a virtual guard");
               additionalGuardInfo->setMergedWithOSRGuard();
               }
            else
               {
               // With the HCR guard removed, the fall through node, which should be the first node of the inlined call,
               // now generates fear
               if (cursor->getNextBlock() && cursor->getNextBlock()->getEntry())
                  fearGeneratingNodes.set(cursor->getNextBlock()->getEntry()->getNode()->getGlobalIndex());
               }

            TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "hcrGuardRemoval/success"), cursor->getExit());
            }
         else if (guardInfo->mergedWithHCRGuard())
            {
            guardInfo->setMergedWithHCRGuard(false);
            if (TR_FearPointAnalysis::virtualGuardsKillFear())
               guardInfo->setMergedWithOSRGuard();
            else
               {
               if (cursor->getNextBlock() && cursor->getNextBlock()->getEntry())
                  fearGeneratingNodes.set(cursor->getNextBlock()->getEntry()->getNode()->getGlobalIndex());
               }
            }
         }
      else
         {
         if (guardInfo->getKind() == TR_HCRGuard)
            {
            TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "hcrGuardRemoval/notsuppoted"), cursor->getLastRealTreeTop());
            }
         }
      }
   }

int32_t TR_OSRGuardInsertion::insertOSRGuards(TR_BitVector &fearGeneratingNodes)
   {
   static char *forceOSRInsertion = feGetEnv("TR_ForceOSRGuardInsertion");

   if (!comp()->getFlowGraph()->getStructure())
      comp()->getFlowGraph()->setStructure(fakeRegion(comp()));

   TR_FearPointAnalysis fearAnalysis(comp(), optimizer(), comp()->getFlowGraph()->getStructure(),
      fearGeneratingNodes, true, trace());
   // Structure is no longer needed after completing the fear analysis and
   // will potentially slow down manipulations to the CFG, so it is remove
   comp()->getFlowGraph()->invalidateStructure();

   TR::TreeTop * cfgEnd = comp()->getFlowGraph()->findLastTreeTop();

   TR::Block *block = NULL;
   TR_SingleBitContainer fear(1, trMemory(), stackAlloc);
   int32_t initialNodeCount = comp()->getNodeCount();
   TR::TreeTop *firstTree = comp()->getStartTree();
   for (TR::TreeTop *cursor = comp()->findLastTree(); cursor; cursor = cursor->getPrevTreeTop())
      {
      if (cursor->getNode()->getOpCodeValue() == TR::BBEnd)
         {
         block = cursor->getNode()->getBlock();
         if (block->isOSRCatchBlock() || block->isOSRCodeBlock())
            {
            cursor = block->getEntry();
            continue;
            }

         // set the fearful state based on all successors - anyone who has an OSR edge is a source
         // of fear and we must add a patch point if we encounter a yield otherwise we are safe
         TR_SuccessorIterator sit(block);
         fear.empty();
         for (TR::CFGEdge *edge = sit.getFirst(); edge; edge = sit.getNext())
            {
            TR::Block *succ = toBlock(edge->getTo());
            if (succ)
                fear |= *(fearAnalysis._blockAnalysisInfo[succ->getNumber()]);
            }

         continue;
         }

      if (TR_FearPointAnalysis::virtualGuardsKillFear()
          && cursor->getNode()->isTheVirtualGuardForAGuardedInlinedCall()
          && comp()->cg()->supportsMergingGuards())
         {
         TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrGuardSummary/patch/vguard/%s/=%d", comp()->getHotnessName(comp()->getMethodHotness()), block->getFrequency()), cursor);
         TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrGuardVGLocation/%s/(%s)/block_%d@%d", comp()->getHotnessName(comp()->getMethodHotness()), comp()->signature(), block->getNumber(), block->getFrequency()), cursor);

         // As HCR guards and other virtual guards will no longer be merged, every virtual guard should have
         // had a HCR guard removed within it, and so they should all have fear reaching them
         if (!fear.isEmpty())
            {
            TR_VirtualGuard *guardInfo = comp()->findVirtualGuardInfo(cursor->getNode());
            TR_ASSERT(guardInfo, "guard info should exist for a virtual guard");
            guardInfo->setMergedWithOSRGuard();
            fear.empty();
            }
         }
      else if (cursor == firstTree && (!fear.isEmpty() || forceOSRInsertion))
         {
         // As the method entry is an implicit OSR point, it is necessary to add a guard here
         // if fear can reach it

         // Hard code the transition target to the first bytecode index in the outermost caller
         TR_ByteCodeInfo nodeBCI;
         nodeBCI.setCallerIndex(-1);
         nodeBCI.setByteCodeIndex(0);
         nodeBCI.setDoNotProfile(false);

         TR::TreeTop *guard = TR::TreeTop::create(comp(), TR_VirtualGuard::createOSRGuard(comp(), NULL));

         // If something went wrong with bookkeeping, due to the nature of the implicit OSR point,
         // this will return false
         bool induceOSR = comp()->allowRecompilation() ? comp()->getMethodSymbol()->induceOSRAfterAndRecompile(cursor, nodeBCI, guard, false, 0, &cfgEnd):
                                                         comp()->getMethodSymbol()->induceOSRAfter(cursor, nodeBCI, guard, false, 0, &cfgEnd);

         if (trace())
            {
            if (induceOSR)
               traceMsg(comp(), "  OSR induction at start of method added successfully\n");
            else
               traceMsg(comp(), "  OSR induction at start of method FAILED!\n");
            }

         TR_ASSERT(induceOSR, "OSR guard insertion must succeed for correctness!");
         }
      else if (comp()->isPotentialOSRPointWithSupport(cursor))
         {
         const char *label = NULL;
         if (cursor->getNode()->getOpCodeValue() == TR::asynccheck)
            label = "asynccheck";
         else if (cursor->getNode()->getFirstChild()->isTheVirtualCallNodeForAGuardedInlinedCall())
            label = "vg_cold_call";
         else if (cursor->getNode()->getOpCodeValue() == TR::monent)
            label = "monent";
         else
            label = "call";
         if (!fear.isEmpty() || forceOSRInsertion)
            {
            if (trace())
               traceMsg(comp(), "Found potential OSR point a n%dn\n", cursor->getNode()->getGlobalIndex());

            // induce OSR in the new block
            TR_ByteCodeInfo nodeBCI = comp()->getMethodSymbol()->getOSRByteCodeInfo(cursor->getNode());
            TR::ResolvedMethodSymbol *targetMethod = nodeBCI.getCallerIndex() == -1 ?
                                                        comp()->getMethodSymbol() :
                                                        comp()->getInlinedResolvedMethodSymbol(nodeBCI.getCallerIndex());

            // with the new HCR we transition after the call and we need to put the transition point
            // after any generated PPS stores
            TR::TreeTop *inductionPoint = comp()->getMethodSymbol()->getOSRTransitionTreeTop(cursor);

            // If this is a monent, it may be followed by a store to a live monitor metadata temp,
            // which may be required for the OSR guard to remat the monitor entry
            TR::TreeTop *nextTree = inductionPoint->getNextTreeTop();
            if (nextTree->getNode()->getOpCode().isStoreDirect()
                && nextTree->getNode()->getOpCode().hasSymbolReference()
                && nextTree->getNode()->getSymbolReference()->getSymbol()->holdsMonitoredObject())
               {
               inductionPoint = nextTree;
               nextTree = nextTree->getNextTreeTop();
               }

            // Shift the induction point past any stores that can be repeated
            // Currently, this is limited to stores of news or calls, as these must have been anchored in a prior
            // tree, either as the OSR point or before it.
            while (nextTree
                && nextTree->getNode()->getOpCode().isStoreDirect()
                && nextTree->getNode()->getOpCode().hasSymbolReference()
                && nextTree->getNode()->getSymbol()->isAutoOrParm()
                && !nextTree->getNode()->getSymbol()->isPendingPush()
                && (nextTree->getNode()->getFirstChild()->getOpCode().isCall() || nextTree->getNode()->getFirstChild()->getOpCode().isNew()))
               {
               inductionPoint = nextTree;
               nextTree = nextTree->getNextTreeTop();
               }

            bool shouldInduce = true;

            static char *disableNormalCallOSRInduction = feGetEnv("TR_disableNormalCallOSRInduction");
            static char *osrNormalGuardThreshold = feGetEnv("TR_osrNormalGuardThreshold");
            static char *osrCallExcludeLoopsOnly = feGetEnv("TR_osrCallExcludeLoopsOnly");
            if (cursor->getNode()->getOpCodeValue() != TR::asynccheck && cursor->getNode()->getOpCodeValue() != TR::monent
                && !cursor->getNode()->getFirstChild()->isTheVirtualCallNodeForAGuardedInlinedCall())
               {
               if (disableNormalCallOSRInduction != NULL)
                  {
                  shouldInduce = false;
                  }
               else if (osrNormalGuardThreshold != NULL
                        && block->getFrequency() >= atoi(osrNormalGuardThreshold))
                  {
                  shouldInduce = !(osrCallExcludeLoopsOnly == NULL || block->getStructureOf()->getContainingLoop());
                  }
               }

            if (shouldInduce)
               {
               TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrGuardSummary/patch/%s/%s/=%d", label, comp()->getHotnessName(comp()->getMethodHotness()), block->getFrequency()), inductionPoint);
               TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrGuardPatchLocation/%s/(%s)/block_%d@%d", comp()->getHotnessName(comp()->getMethodHotness()), comp()->signature(), block->getNumber(), block->getFrequency()), inductionPoint);
               if (cursor->getNode()->getOpCodeValue() == TR::asynccheck)
                  TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrAsynccheckPatch/%s/(%s)/(%s)/block_%d@%d", comp()->getHotnessName(comp()->getMethodHotness()), nodeBCI.getCallerIndex() > -1 ? comp()->getInlinedResolvedMethod(nodeBCI.getCallerIndex())->signature(trMemory()): comp()->signature(), comp()->signature(), block->getNumber(), block->getFrequency()), inductionPoint);
               else if (cursor->getNode()->getOpCodeValue() == TR::monent)
                  TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrMonentPatch/%s/(%s)/(%s)/block_%d@%d", comp()->getHotnessName(comp()->getMethodHotness()), nodeBCI.getCallerIndex() > -1 ? comp()->getInlinedResolvedMethod(nodeBCI.getCallerIndex())->signature(trMemory()): comp()->signature(), comp()->signature(), block->getNumber(), block->getFrequency()), inductionPoint);
               else if (!cursor->getNode()->getFirstChild()->isTheVirtualCallNodeForAGuardedInlinedCall())
                  TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrCallPatch/%s/(%s)/(%s)/block_%d@%d", comp()->getHotnessName(comp()->getMethodHotness()), nodeBCI.getCallerIndex() > -1 ? comp()->getInlinedResolvedMethod(nodeBCI.getCallerIndex())->signature(trMemory()): comp()->signature(), comp()->signature(), block->getNumber(), block->getFrequency()), inductionPoint);
               TR::TreeTop *guard = TR::TreeTop::create(comp(),TR_VirtualGuard::createOSRGuard(comp(), NULL));

               // Modify the bytecode info for the guard's children to match the
               // yield point. This prevents confusion with value profiling.
               TR_ByteCodeInfo guardBCI = nodeBCI;
               guardBCI.setDoNotProfile(true);
               guard->getNode()->getFirstChild()->setByteCodeInfo(guardBCI);
               guard->getNode()->getSecondChild()->setByteCodeInfo(guardBCI);

               bool induceOSR = comp()->allowRecompilation() ? targetMethod->induceOSRAfterAndRecompile(inductionPoint, nodeBCI, guard, false, comp()->getOSRInductionOffset(cursor->getNode()), &cfgEnd):
                                                               targetMethod->induceOSRAfter(inductionPoint, nodeBCI, guard, false, comp()->getOSRInductionOffset(cursor->getNode()), &cfgEnd);

               if (trace() && induceOSR)
                  traceMsg(comp(), "  OSR induction added successfully\n");
               else if (trace())
                  traceMsg(comp(), "  OSR induction FAILED!\n");

               TR_ASSERT(induceOSR, "OSR guard insertion must succeed for correctness!");

               if (!comp()->getOption(TR_DisableOSRLocalRemat))
                  performRemat(cursor, guard, guard->getNode()->getBranchDestination());
               }
            else
               {
               TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrGuardSummary/exclude/%s/%s/=%d", label, comp()->getHotnessName(comp()->getMethodHotness()), block->getFrequency()), inductionPoint);
               if (trace())
                  traceMsg(comp(), "  OSR induction skipped to env var");
               }
            fear.empty();
            }
         else
            {
            TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrGuardSummary/nofear/%s/%s/=%d", label, comp()->getHotnessName(comp()->getMethodHotness()), block->getFrequency()), cursor);
            if (cursor->getNode()->getGlobalIndex() < initialNodeCount)
               fear |= *fearAnalysis.generatedFear(cursor->getNode());
            }
         }
      else if (cursor->getNode()->getOpCodeValue() == TR::asynccheck)
         {
         TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrGuardSummary/whitelist/asynccheck/%s/=%d", comp()->getHotnessName(comp()->getMethodHotness()), block->getFrequency()), cursor);
         }
      else
         {
         if (cursor->getNode()->getGlobalIndex() < initialNodeCount)
            fear |= *fearAnalysis.generatedFear(cursor->getNode());
         }
      }

   // HCR in the new world is only allowed to happen at three kinds of async points:
   // calls, asyncchecks and monents.
   // HCR and FSD now work in a similar way - ilgen will add exception edges to
   // the OSR catch-block to prevent optimization based on the isPotentialOSRPoint query
   // which returns true in HCR mode for calls and asyncchecks
   // Unlike FSD the JIT controls OSR transitions for HCR so exception edges to the OSR
   // catch block have to either be (a) removed because there is no longer an async point
   // in the block or (b) a guard and an induce have to be added
   // After this pass adding potentialOSRPoints to HCR compiles is illegal and dangerous
   return 1;
}


void TR_OSRGuardInsertion::generateTriggeringRecompilationTrees(TR::TreeTop *osrGuard, TR_PersistentMethodInfo::InfoBits reason)
   {
   if (comp()->isRecompilationEnabled() && !comp()->getOption(TR_DisableRecompDueToInlinedMethodRedefinition))
      {
      TR::TreeTop *osrInduceBlockStart = osrGuard->getNode()->getBranchDestination();
      TR::TreeTop *callTree = TR::TransformUtil::generateRetranslateCallerWithPrepTrees(osrInduceBlockStart->getNode(), reason, comp());
      osrInduceBlockStart->insertTreeTopsAfterMe(callTree);
      }
   }

/*
 * This will remat as many of the live symrefs across the guard as possible.
 *
 * osrPoint: The treetop containing the OSR point.
 * osrGuard: The treetop containing the OSR guard.
 * rematDest: The first treetop in the OSR induce block.
 */
void TR_OSRGuardInsertion::performRemat(TR::TreeTop *osrPoint, TR::TreeTop *osrGuard,
   TR::TreeTop *rematDest)
   {
   static const char *p = feGetEnv("TR_OSRRematBlockLimit");
   static uint32_t rematBlockLimit = p ? atoi(p) : defaultRematBlockLimit;

   // The block containing the OSR point and the guard
   TR::Block *osrBlock = osrPoint->getEnclosingBlock()->startOfExtendedBlock();
   TR::TreeTop *osrStart = osrGuard->getPrevTreeTop();

   // Specify the extended block
   TR::TreeTop *start;
   TR_BitVector *blocksToVisit = new (trStackMemory()) TR_BitVector(
      comp()->getFlowGraph()->getNextNodeNumber(), trMemory(), stackAlloc);
   blocksToVisit->set(osrBlock->getNumber());

   TR::SparseBitVector scanTargets(comp()->allocator());
   RematSafetyInformation safetyInfo(comp());
   TR::list<TR::TreeTop *> failedArgs(getTypedAllocator<TR::TreeTop*>(comp()->allocator()));

   // When collecting remat candidates, we don't want any of the stores that the induction point
   // skipped past, which will be performed twice.
   //
   TR::TreeTop *realStart = osrStart;
   while (realStart->getNode()->getOpCode().isStoreDirect() &&
          !realStart->getNode()->getSymbol()->isPendingPush())
      realStart = realStart->getPrevTreeTop();

   // Collect remat candidates
   //
   uint32_t rematBlocks = 1;
   TR::SparseBitVector seen(comp()->allocator());
   for (TR::TreeTop *cursor = realStart; cursor; cursor = cursor->getPrevTreeTop())
      {
      // Check if there is a block to go to
      TR::Node *node = cursor->getNode();
      if (node->getOpCodeValue() == TR::BBStart)
         {
         start = cursor;
         TR::Block *block = cursor->getNode()->getBlock();
         if (block->getPredecessors().size() + block->getExceptionPredecessors().size() != 1)
            {
            if (trace())
               traceMsg(comp(), "   block_%d has multiple predecessors, starting remat\n", block->getNumber());
            break;
            }
         else if (rematBlocks > rematBlockLimit)
            {
            if (trace())
               traceMsg(comp(), "   remat block limit %d reached at block_%d, starting remat\n", rematBlockLimit, block->getNumber());
            break;
            }

         TR::Block *next;
         if (!block->getPredecessors().empty())
            next = block->getPredecessors().front()->getFrom()->asBlock();
         else
            next = block->getExceptionPredecessors().front()->getFrom()->asBlock();

         if (next == comp()->getFlowGraph()->getStart())
            {
            if (trace())
               traceMsg(comp(), "   predecessor to block_%d is start, starting remat\n", block->getNumber());
            break;
            }

         if (trace())
            traceMsg(comp(), "   considering block_%d before block_%d for OSR remat\n",
               next->getNumber(), block->getNumber());

         // Count non-empty blocks
         if (next->getFirstRealTreeTop() != next->getExit())
            {
            rematBlocks++;
            if (trace())
               traceMsg(comp(), "   block_%d added as block #%d\n", next->getNumber(), rematBlocks);
            }
         else if (trace())
            traceMsg(comp(), "   block_%d is empty\n", next->getNumber());

         // Shift the cursor to the end of the next block
         cursor = next->getExit();
         blocksToVisit->set(next->getNumber());
         }

      if (node->getOpCode().isStoreDirect() && !seen[node->getSymbolReference()->getReferenceNumber()])
         {
         if (trace())
            traceMsg(comp(), "  considering store node [%p] - %d - for remat\n", node, node->getGlobalIndex());
         TR::SparseBitVector argSymRefsToCheck(comp()->allocator());
         TR_YesNoMaybe result = RematTools::gatherNodesToCheck(comp(), node, node->getFirstChild(),
            scanTargets, argSymRefsToCheck, trace());
         seen[node->getSymbolReference()->getReferenceNumber()] = true;

         if (result == TR_yes)
            {
            if (trace())
               traceMsg(comp(),"    remat may be possible for node [%p] - %d\n", node, node->getGlobalIndex());
            safetyInfo.add(cursor, argSymRefsToCheck);
            }
         else if (result == TR_no)
            {
            if (trace())
               traceMsg(comp(),"    remat unsafe for node [%p] - %d\n", node, node->getGlobalIndex());
            failedArgs.push_back(cursor);
            }
         else if (result == TR_maybe)
            {
            // constants will be ignored and dealt with by constant propagation
            if (trace())
               traceMsg(comp(),"    ignoring constant node [%p] - %d\n", node, node->getGlobalIndex());
            }
         }
      }

   // if we have failed to remat any arguments we want to see if there is another
   // store of the argument that we can use for partial remat purposes - hibb often
   // makes these for us
   if (failedArgs.size() > 0)
      {
      RematTools::walkTreeTopsCalculatingRematFailureAlternatives(comp(),
         start, osrGuard, failedArgs, scanTargets, safetyInfo, blocksToVisit, trace());

      for (auto iter = failedArgs.begin(); iter != failedArgs.end(); ++iter)
         {
         // NULL means we actually do have a candidate load to try and to
         // partially rematerialize the argument
         if (!*iter)
            continue;

         TR::TreeTop *storeTree = *iter;
         TR::Node *store = storeTree->getNode();
         if (trace())
            traceMsg(comp(), "Failed to find failure alternative node [%p]\n", store);

         TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrGuardRemat/Failed/%s",
            store->getFirstChild()->getOpCode().getName()), storeTree, 1, TR::DebugCounter::Expensive);
         TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrGuardRemat.byMethod/Failed/(%s)/%s",
            comp()->signature(), store->getFirstChild()->getOpCode().getName()));
         TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrGuardRemat.byReason/%s",
            store->getFirstChild()->getOpCode().getName()));
         }
      }

   TR::SparseBitVector unsafeSymRefs(comp()->allocator());
   if (!scanTargets.IsZero())
      RematTools::walkTreesCalculatingRematSafety(comp(),
         start, osrGuard, scanTargets, unsafeSymRefs, blocksToVisit, trace());

   if (trace())
      safetyInfo.dumpInfo(comp());

   for (int32_t i = safetyInfo.size() - 1; i >= 0; --i)
      {
      TR::TreeTop *storeTree = safetyInfo.argStore(i);
      TR::TreeTop *rematTree = safetyInfo.rematTreeTop(i);
      TR::Node *store = storeTree->getNode();
      if (!unsafeSymRefs.Intersects(safetyInfo.symRefDependencies(i)))
         {
         if (trace())
            traceMsg(comp(), "Found safe remat candidate. store: [%p] remat: [%p]\n", store, rematTree->getNode());

         TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrGuardRemat.byMethod/(%s)/Succeeded",
            comp()->signature()));
         TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrGuardRemat.byReason/Success"));
         TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrGuradRemat/Succeeded/(%s)",
            store->getFirstChild()->getOpCode().getName()), storeTree, 1, TR::DebugCounter::Expensive);
         // equality of rematTree and storeTree means we want to duplicate the computation of
         // the argument and do a full rematerialization
         if (rematTree == storeTree)
            {
            TR::Node *duplicateStore = store->duplicateTree();
            if (performTransformation(comp(), "O^O GUARDED CALL REMAT: Rematerialize [%p] as [%p]\n", store, duplicateStore))
               {
               rematDest = TR::TreeTop::create(comp(), rematDest, duplicateStore);
               }
            }
         else
            {
            TR::Node *duplicateStore = TR::Node::createStore(store->getSymbolReference(), TR::Node::createLoad(store, rematTree->getNode()->getSymbolReference()));
            duplicateStore->setByteCodeInfo(store->getByteCodeInfo());
            if (performTransformation(comp(), "O^O GUARDED CALL REMAT: Partial rematerialize of [%p] as [%p] - load of [%d]\n", store,
                duplicateStore, rematTree->getNode()->getSymbolReference()->getReferenceNumber()))
               {
               rematDest = TR::TreeTop::create(comp(), rematDest, duplicateStore);
               }
            }
         }
      else
         {
         if (trace())
            traceMsg(comp(), "Unsafe to remat [%p]\n", store);

         TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrGuardRemat.byMethod/unsafeSymRef/Failed/(%s)/%s",
            comp()->signature(), store->getFirstChild()->getOpCode().getName()));
         TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrGuardRemat.byReason/unsafeSymRef/%s",
            store->getFirstChild()->getOpCode().getName()));
         TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrGuardRemat/Failed/(%s)",
            store->getFirstChild()->getOpCode().getName()), storeTree, 1, TR::DebugCounter::Expensive);
         }
      }
   }

void TR_OSRGuardInsertion::collectFearFromOSRFearPointHelperCalls(TR_BitVector &fearGeneratingNodes, TR_HCRGuardAnalysis* guardAnalysis)
   {
   bool protectedByOSRPoints = false;
   TR::NodeChecklist visited(comp());

   for (TR::TreeTop *treeTop = comp()->getStartTree();
        treeTop;
        treeTop = treeTop->getNextRealTreeTop())
      {
      TR::Node *ttNode = treeTop->getNode();

      if (ttNode->getOpCodeValue() == TR::BBStart)
         {
         TR::Block* block = treeTop->getEnclosingBlock();
         protectedByOSRPoints = !guardAnalysis || guardAnalysis->_blockAnalysisInfo[block->getNumber()]->isEmpty();
         continue;
         }

      TR::Node *osrNode = NULL;
      if (comp()->isPotentialOSRPoint(ttNode, &osrNode))
         {
         if (visited.contains(osrNode))
            continue;

         if (comp()->isPotentialOSRPointWithSupport(treeTop))
            {
            if (!protectedByOSRPoints && trace())
               traceMsg(comp(), "treetop n%dn is an OSR point with support\n", ttNode->getGlobalIndex());
            protectedByOSRPoints = true;
            }
         else
            {
            if (protectedByOSRPoints && trace())
               traceMsg(comp(), "treetop n%dn is an OSR point without support\n", ttNode->getGlobalIndex());
            protectedByOSRPoints = false;
            }
         visited.add(osrNode);
         continue;
         }

      if (ttNode->getNumChildren() == 0)
         continue;

      TR::Node *node = ttNode->getFirstChild();
      if (node &&
          node->isOSRFearPointHelperCall())
         {
         static char *assertOnFearPointWithoutProtection = feGetEnv("TR_AssertOnFearPointWithoutProtection");
         if (assertOnFearPointWithoutProtection)
            {
            TR_ASSERT_FATAL(protectedByOSRPoints, "A fear point node %p n%dn [%d,%d] is reached by unsupported potential OSR point\n",
               node, node->getGlobalIndex(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeIndex());
            }

         fearGeneratingNodes.set(ttNode->getGlobalIndex());
         }
      }
   }

void TR_OSRGuardInsertion::cleanUpOSRFearPoints()
   {
   for (TR::TreeTop *treeTop = comp()->getStartTree();
        treeTop;
        treeTop = treeTop->getNextRealTreeTop())
      {
      TR::Node *ttNode = treeTop->getNode();
      if (ttNode->getNumChildren() == 1 &&
          ttNode->getFirstChild()->isOSRFearPointHelperCall())
         {
         dumpOptDetails(comp(), "Remove osrFearPointHelper call n%dn %p\n", ttNode->getGlobalIndex(), ttNode);
         TR::TreeTop* prevTree = treeTop->getPrevTreeTop();
         TR::TransformUtil::removeTree(comp(), treeTop);
         treeTop = prevTree;
         }
      }
   }
