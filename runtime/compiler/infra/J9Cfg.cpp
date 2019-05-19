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

#include <algorithm>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "cs2/arrayof.h"
#include "cs2/bitvectr.h"
#include "cs2/listof.h"
#include "env/TRMemory.hpp"
#include "env/PersistentInfo.hpp"
#include "env/VMJ9.h"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/Stack.hpp"
#include "infra/TRCfgEdge.hpp"
#include "infra/TRCfgNode.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/StructuralAnalysis.hpp"
#include "ras/Debug.hpp"
#include "runtime/ExternalProfiler.hpp"
#include "runtime/J9Profiler.hpp"

#ifdef __MVS__
#include <stdlib.h>
#endif


static TR_PersistentProfileInfo *getProfilingInfoForCFG(TR::Compilation *comp, TR::CFG *cfg)
   {
   TR_PersistentProfileInfo *info = TR_PersistentProfileInfo::get(comp);
   if (cfg == comp->getFlowGraph()
       && comp->getRecompilationInfo())
      return info;

   if ((*(TR_BlockFrequencyInfo::getEnableJProfilingRecompilation())) == -1 
       && cfg->getMethodSymbol()
       && cfg->getMethodSymbol()->getResolvedMethod()
       && info
       && info->getBlockFrequencyInfo()
       && info->getBlockFrequencyInfo()->isJProfilingData())
      {
      return info;
      }
   return NULL;
   }

static bool hasBlockFrequencyInfo(TR::Compilation *comp, TR::CFG *cfg)
   {
   TR_PersistentProfileInfo *profileInfo = getProfilingInfoForCFG(comp, cfg);
   return profileInfo && profileInfo->getBlockFrequencyInfo();
   }

static bool hasJProfilingInfo(TR::Compilation *comp, TR::CFG *cfg)
   {
   static char *disableJProfilingForInner = feGetEnv("TR_disableJProfilingForInner");
   TR_PersistentProfileInfo *profileInfo = getProfilingInfoForCFG(comp, cfg);
   if (disableJProfilingForInner == NULL
       && profileInfo
       && profileInfo->getBlockFrequencyInfo()
       && profileInfo->getBlockFrequencyInfo()->isJProfilingData()
       && ((*(TR_BlockFrequencyInfo::getEnableJProfilingRecompilation())) == -1))
      {
      TR_ByteCodeInfo toCheck;
      toCheck.setByteCodeIndex(0);
      toCheck.setCallerIndex(comp->getCurrentInlinedSiteIndex());
      int32_t entryFrequency = profileInfo->getBlockFrequencyInfo()->getFrequencyInfo(toCheck, comp, false, false);
      if (entryFrequency > -1)
         {
         return true;
         }
      }
   return false;
   }

bool
J9::CFG::setFrequencies()
   {
   if (this == comp()->getFlowGraph() && !comp()->getRecompilationInfo())
      {
      resetFrequencies();
      }
   _max_edge_freq = MAX_PROF_EDGE_FREQ;

   TR_ExternalProfiler *profiler;

   // Do not use JIT profiler info for estimate code size.
   bool externFreq = ! comp()->getOption(TR_EnableScorchInterpBlockFrequencyProfiling);
   bool hasJPI = hasJProfilingInfo(comp(), self());
   if (externFreq
       && comp()->hasBlockFrequencyInfo()
       && (
           (!hasJPI && (this == comp()->getFlowGraph()))
           || (hasJPI && ((*(TR_BlockFrequencyInfo::getEnableJProfilingRecompilation())) == -1))
          )
      )
      {
      if (!self()->consumePseudoRandomFrequencies())
         {
         _externalProfiler = comp()->fej9()->hasIProfilerBlockFrequencyInfo(*comp());
         TR_BitVector *nodesToBeNormalized = self()->setBlockAndEdgeFrequenciesBasedOnJITProfiler();
         self()->normalizeFrequencies(nodesToBeNormalized);
         if (comp()->getOption(TR_TraceBFGeneration))
            {
            traceMsg(comp(), "CFG of %s after setting frequencies using JITProfiling\n", self()->getMethodSymbol()->signature(comp()->trMemory()));
            comp()->dumpFlowGraph(self());
            }
         if (this == comp()->getFlowGraph() && comp()->getInlinedCalls() > 0)
            {
            for (TR::Block *block = comp()->getStartBlock(); block; block = block->getNextBlock())
               {
               if (!block->getEntry() || !block->getEntry()->getNode())
                  {
                  continue;
                  }
               TR_ByteCodeInfo &bci = block->getEntry()->getNode()->getByteCodeInfo();
               TR::ResolvedMethodSymbol *inlinedMethod = bci.getCallerIndex() == -1 ? comp()->getMethodSymbol() : comp()->getInlinedResolvedMethodSymbol(bci.getCallerIndex());
               }
            }
         }

      if (comp()->getOption(TR_VerbosePseudoRandom))
         emitVerbosePseudoRandomFrequencies();

      return true;
      }
   else if ((profiler = comp()->fej9()->hasIProfilerBlockFrequencyInfo(*comp())))
      {
      if (!self()->consumePseudoRandomFrequencies())
         {
         profiler->setBlockAndEdgeFrequencies(self(), comp());
         if (self()->getMethodSymbol())
            {
            TR::CFGNode *nextNode = self()->getFirstNode();
            for (; nextNode != NULL; nextNode = nextNode->getNext())
               {
               if (nextNode->asBlock()->getEntry()
                   && self()->getMethodSymbol()->getProfilerFrequency(nextNode->asBlock()->getEntry()->getNode()->getByteCodeIndex()) < 0)
                  self()->getMethodSymbol()->setProfilerFrequency(nextNode->asBlock()->getEntry()->getNode()->getByteCodeIndex(), nextNode->asBlock()->getFrequency());
               }
            }
         }

      if (comp()->getOption(TR_VerbosePseudoRandom))
         emitVerbosePseudoRandomFrequencies();

      return true;
      }
   else if (comp()->getFlowGraph()->getStructure() && (comp()->getFlowGraph() == self()))
      {
      if (!self()->consumePseudoRandomFrequencies())
         {
         _max_edge_freq = MAX_STATIC_EDGE_FREQ;
         self()->setBlockAndEdgeFrequenciesBasedOnStructure();
         if (comp()->getOption(TR_TraceBFGeneration))
            comp()->dumpMethodTrees("Trees after setting frequencies from structures", comp()->getMethodSymbol());
         }

      if (comp()->getOption(TR_VerbosePseudoRandom))
         emitVerbosePseudoRandomFrequencies();

      return true;
      }

   return false;
   }


#define GUESS_THRESHOLD 100

static bool isVirtualGuard(TR::Node *ifNode)
   {
   return (ifNode->isTheVirtualGuardForAGuardedInlinedCall() || ifNode->isProfiledGuard());
   }


TR_BitVector *
J9::CFG::setBlockAndEdgeFrequenciesBasedOnJITProfiler()
   {
   TR_PersistentProfileInfo *profileInfo = getProfilingInfoForCFG(comp(), self());

   if (!profileInfo)
      return NULL;

   TR_BlockFrequencyInfo *blockFrequencyInfo = profileInfo->getBlockFrequencyInfo();

   int32_t maxCount = profileInfo->getMaxCount();

   TR_BitVector *nodesToBeNormalized = NULL;
   TR::CFGNode *node;

   int32_t *nodeFrequencies = NULL;
   if (_maxFrequency < 0)
      {
      nodeFrequencies = (int32_t*) trMemory()->allocateStackMemory(sizeof(int32_t) * self()->getNextNodeNumber());
      for (node = getFirstNode(); node; node = node->getNext())
         {
         int32_t nodeNumber = toBlock(node)->getNumber();
         nodeFrequencies[nodeNumber] = blockFrequencyInfo->getFrequencyInfo(toBlock(node), comp());
         if ((nodeFrequencies[nodeNumber] >= _maxFrequency) && (nodeFrequencies[nodeNumber] >= 0))
            _maxFrequency = nodeFrequencies[nodeNumber];
         }
      }

   int32_t origMaxFrequency = _maxFrequency;

   createTraversalOrder(true, stackAlloc);

   TR::CFGNode *nextNode = getFirstNode();
   for (; nextNode != NULL; nextNode = nextNode->getNext())
       {
       TR_SuccessorIterator sit(nextNode);
       TR::CFGEdge * edge = sit.getFirst();
       for(; edge != NULL; edge=sit.getNext())
           {
           if (comp()->getOption(TR_TraceBFGeneration))
              traceMsg(comp(), "edge visit count = %d\n", edge->getVisitCount());
           edge->setVisitCount(1);
           }
       }

   for (int32_t traversalIndex = 0; traversalIndex < getForwardTraversalLength(); traversalIndex++)
     {
     node = getForwardTraversalElement(traversalIndex);
     int32_t frequency = node->getFrequency();
     if (frequency < 0)
        {
        frequency = nodeFrequencies ? nodeFrequencies[toBlock(node)->getNumber()] : blockFrequencyInfo->getFrequencyInfo(toBlock(node), comp());
        //frequency = nodeFrequencies[toBlock(node)->getNumber()];

        bool isGuardedBlock = false;
        bool isProfiledGuard = false;
        bool isGuardedBlockFallThrough = false;
        int32_t combinedPredRawFrequency = 0;
        TR_PredecessorIterator pit(node);
        TR::CFGEdge *edge;
        for (edge = pit.getFirst(); edge; edge = pit.getNext())
            {
            TR::Block *block = edge->getFrom()->asBlock();
            TR::TreeTop *lastTree = NULL;
            if (block->getExit())
               lastTree = block->getLastRealTreeTop();
            if (lastTree &&
                lastTree->getNode()->getOpCode().isIf() &&
                isVirtualGuard(lastTree->getNode()))
               {
               if (lastTree->getNode()->getBranchDestination() == node->asBlock()->getEntry())
                  {
                  isGuardedBlock = true;
                  if (lastTree->getNode()->isProfiledGuard())
                     isProfiledGuard = true;
                  }
               else
                  isGuardedBlockFallThrough = true;
               }
            }

        // Infer a frequency based on frequencies of predecessors.
        // We can use this if we have nothing better.
        //
        for (edge = pit.getFirst(); edge; edge = pit.getNext())
            {
            TR::Block *pred = edge->getFrom()->asBlock();

            // Compute this predecessor's raw frequency
            //
            int32_t predRawFrequency = pred->getFrequency();
            if (predRawFrequency < 0)
               {
               // This should be unusual.  Getting here means we haven't
               // processed a predecessor, which (due to our reverse postorder
               // traversal) means we're in a loop.  Even then, loops usually
               // have pretty good raw profiling info, so we still usually don't
               // see -1.  However, it can happen, so let's make some attempt to
               // get a decent guess at a frequency.
               //
               predRawFrequency = blockFrequencyInfo->getFrequencyInfo(pred, comp());
               predRawFrequency = std::max(predRawFrequency, 0);
               predRawFrequency = std::min(predRawFrequency, maxCount);
               }
            else if (nodesToBeNormalized && nodesToBeNormalized->isSet(pred->getNumber()))
               {
               // Frequency is already raw; leave it alone
               }
            else
               {
               predRawFrequency = TR::CFGNode::denormalizedFrequency(predRawFrequency, origMaxFrequency);
               }

            bool effectiveSingleton = false;
            TR::TreeTop *lastTree = NULL;
            if (pred && pred->getExit())
               lastTree = pred->getLastRealTreeTop();
            if (lastTree &&
                lastTree->getNode()->getOpCode().isIf())
               {
               TR::Block *fallThrough = pred->getNextBlock();
               TR::Block *branchTarget = lastTree->getNode()->getBranchDestination()->getEnclosingBlock();
               if (fallThrough && branchTarget)
                  {
                  if (comp()->getOption(TR_TraceBFGeneration))
                     traceMsg(comp(), "      checking effective singleton on block_%d with fallthrough block_%d and branchTarget block_%d\n", pred->getNumber(), fallThrough->getNumber(), branchTarget->getNumber());
                  if (fallThrough == node && !fallThrough->isCold() && branchTarget->isCold())
                     effectiveSingleton = true;
                  else if (branchTarget == node && !branchTarget->isCold() && fallThrough->isCold())
                     effectiveSingleton = true;
                  }
               }

            if (comp()->getOption(TR_TraceBFGeneration) && pred)
               traceMsg(comp(), "      block_%d's pred block_%d has normalized frequency %d and %s\n",
                  node->getNumber(),
                  pred->getNumber(),
                  predRawFrequency,
                  (pred->getSuccessors().size() == 1)? "one successor" : (effectiveSingleton ? "one effective successor" : "multiple successors"));

            // Use predecessor's frequency as appropriate
            //
            if (((pred->getSuccessors().size() == 1) && pred->hasSuccessor(node)) || effectiveSingleton)
               {
               combinedPredRawFrequency += predRawFrequency;
               if (comp()->getOption(TR_TraceBFGeneration))
                  traceMsg(comp(), "       combinedPredRawFrequency is now %d\n", combinedPredRawFrequency);
               }
            else if (isGuardedBlock)
               {
               if (comp()->getOption(TR_TraceBFGeneration))
                  traceMsg(comp(), "       %d is a guarded block; ignoring predecessor\n", node->getNumber());
               }
            else if (pred && pred->hasSuccessor(node) && (frequency < 0 || (!blockFrequencyInfo->isJProfilingData() && pred->getFrequency() <= GUESS_THRESHOLD)))
               {
               int32_t succCount = 0;
               for (auto edge = pred->getSuccessors().begin(); edge != pred->getSuccessors().end(); ++edge)
                  {
                  if ((*edge)->getTo() && (*edge)->getTo()->asBlock() && node->asBlock()
                      && (*edge)->getTo()->asBlock() != node->asBlock()
                      && (*edge)->getTo()->getFrequency() > -1)
                     predRawFrequency -= (*edge)->getTo()->getFrequency();
                  else
                     succCount++;
                  }
               if (predRawFrequency > 0)
                  {
                  combinedPredRawFrequency += succCount ? (predRawFrequency / succCount) : predRawFrequency;
                  if (comp()->getOption(TR_TraceBFGeneration))
                     traceMsg(comp(), "       combinedPredRawFrequency is now %d based on succCount %d and predRawFrequency %d\n", combinedPredRawFrequency, succCount, predRawFrequency);
                  }
               else
                  {
                  // Can't figure out how often we get here from this pred, and can't ignore it.
                  // Fall back on old wild-guess heuristic.
                  // NOTE: this uses a normalized frequency where it should be a
                  // raw one, but this bug has been in Java6 for ages, so we don't
                  // want to change in an SR.
                  //
                  combinedPredRawFrequency += origMaxFrequency / 4; // probably intended something like denormalizedFrequency(25,100)
                  if (comp()->getOption(TR_TraceBFGeneration))
                     traceMsg(comp(), "       can't figure out frequency; defaulting to wild guess %d\n", combinedPredRawFrequency);
                  }
               }
            else if (frequency < 0 || (!blockFrequencyInfo->isJProfilingData() && pred->getFrequency() <= GUESS_THRESHOLD))
               {
               // Can't figure out how often we get here from this pred, and can't ignore it.
               // Fall back on old wild-guess heuristic.
               // NOTE: this uses a normalized frequency where it should be a
               // raw one, but this bug has been in Java6 for ages, so we don't
               // want to change in an SR.
               //
               combinedPredRawFrequency = origMaxFrequency / 4; // probably intended something like denormalizedFrequency(25,100)
               if (comp()->getOption(TR_TraceBFGeneration))
                  traceMsg(comp(), "       can't figure out frequency; defaulting to wild guess %d\n", combinedPredRawFrequency);
               break;
               }
            }

        combinedPredRawFrequency = std::min(maxCount, combinedPredRawFrequency);

        if (_compilation->getOption(TR_TraceBFGeneration))
           traceMsg(comp(), "Raw frequency for block_%d is %d (maxCount %d origMax %d combinedPredRawFrequency %d)\n", node->getNumber(), frequency, maxCount, origMaxFrequency, combinedPredRawFrequency);

        if (frequency <= 0)
           frequency = combinedPredRawFrequency;

        if (frequency > maxCount)
           frequency = maxCount;

        if (isGuardedBlock)
           {
           if (isProfiledGuard)
              frequency = MAX_COLD_BLOCK_COUNT+1;
           else
              frequency = VERSIONED_COLD_BLOCK_COUNT;
           }


        //if (!isGuardedBlock)
           {
           if (comp()->getOption(TR_TraceBFGeneration))
              traceMsg(comp(), "   Setting block_%d frequency %d (origMaxFrequency %d)\n", node->getNumber(), frequency, origMaxFrequency);
           if ((frequency > origMaxFrequency) &&
               (origMaxFrequency > -1))
              {
              if (!isGuardedBlock)
                 {
                 if (!nodesToBeNormalized)
                    nodesToBeNormalized = new (trStackMemory()) TR_BitVector(getNextNodeNumber(), trMemory(), stackAlloc);

                 nodesToBeNormalized->set(node->getNumber());
                 //dumpOptDetails(comp(),"NOT normalized %d freq %d\n", node->getNumber(), frequency);
                 }
              node->setFrequency(frequency);
              }
           else
              {
              node->setFrequency(frequency);
              if (!isGuardedBlock)
                 node->normalizeFrequency(frequency, origMaxFrequency);
              //dumpOptDetails(comp(),"Normalized %d freq %d\n", node->getNumber(), node->getFrequency());
              }
           }
        //else
        //   node->setFrequency(frequency);
        }

     // DORIT: node->frequency is finalized and set. See if can infer anything about edge frequency:
     // visitCount>1 indicates that a final frequency for the edge had been set.
     frequency = node->getFrequency();
     if (frequency >= 0 && (node->getSuccessors().size() == 1) && (node->getSuccessors().front()->getTo()->getPredecessors().size() == 1))
        {
        TR::CFGEdge *edge = node->getSuccessors().front();
        TR::Block *succ = edge->getTo()->asBlock();

        if (comp()->getOption(TR_TraceBFGeneration))
           traceMsg(comp(), "node %d has single succ. set Frequency for edge %d->%d to %d final.\n", node->getNumber(), node->getNumber(), succ->getNumber(), frequency);

        if (edge->getVisitCount()>1)
           {
           if (comp()->getOption(TR_TraceBFGeneration))
              traceMsg(comp(), "edge visitCount=%d.\n",edge->getVisitCount());
           }
        else
           {
           edge->setFrequency(frequency);
           edge->setVisitCount(2);
           }
         }

      if (frequency >= 0 && (node->getPredecessors().size() == 1) && (node->getPredecessors().front()->getFrom()->getSuccessors().size() == 1))
         {
         TR::CFGEdge *edge = node->getPredecessors().front();
         TR::Block *pred = edge->getFrom()->asBlock();

         if (comp()->getOption(TR_TraceBFGeneration))
            traceMsg(comp(), "node %d has single pred. set Frequency for edge %d->%d to %d final.\n", node->getNumber(), pred->getNumber(), node->getNumber(), frequency);

         if (edge->getVisitCount()>1)
            {
            if (comp()->getOption(TR_TraceBFGeneration))
               traceMsg(comp(), "edge visitCount=%d.\n",edge->getVisitCount());
            }
         else
            {
            edge->setFrequency(frequency);
            edge->setVisitCount(2);
            }
         }
     }

   if (nodesToBeNormalized)
      {
      for (node = getFirstNode(); node; node = node->getNext())
         {
         int32_t frequency = node->getFrequency();
         if (!nodesToBeNormalized->get(node->getNumber()) &&
             !node->asBlock()->isCold())
            {
            nodesToBeNormalized->set(node->getNumber());
            //dumpOptDetails(comp(),"denormalizing node %d (BEFORE) with freq %d\n", node->getNumber(), frequency);
            frequency = node->denormalizeFrequency(origMaxFrequency);
            //dumpOptDetails(comp(),"denormalizing node %d (AFTER) with freq %d\n", node->getNumber(), frequency);
            }

         if (frequency > _maxFrequency)
            _maxFrequency = frequency;
         //dumpOptDetails(comp(), "_maxFrequency = %d\n", _maxFrequency);
         }
      }

   //dumpOptDetails(comp(), "_maxFrequency = %d\n", _maxFrequency);

   _maxEdgeFrequency = -1;

   // Turn off this loop code below when propagation is
   // fixed
   //
   for (node = getFirstNode(); node; node = node->getNext())
      {
      int32_t frequency = node->getFrequency();
      if (frequency >= 0)
         {
         int32_t successorFrequency = 0;
         for (auto e = node->getSuccessors().begin(); e != node->getSuccessors().end(); ++e)
            {
            int32_t normalizedFrequency = (*e)->getTo()->getFrequency();

            // DORIT: try to infer about edge frequencies
            // visitCount>1 indicates that a final frequency for the edge had been set.
            if ((*e)->getVisitCount() > 1)
               {
               // don't add to successorFrequency sum;
               // peek at "cousin" blocks to see if frequency of other edges can be determined.
               // step1: if this successor has only one other predecessor...:
               TR::Block *succ = (*e)->getTo()->asBlock();
               if (succ->getPredecessors().size() == 2) //succ has only one other predecessor
                  {
                  TR::CFGEdge *ee = succ->getPredecessors().front();
                  if (ee->getFrom() == node)
                      ee = *(++(succ->getPredecessors().begin()));
                  if ((succ->getFrequency() >= 0) && (succ->getFrequency() >= (*e)->getFrequency()))
                     {
                     int32_t ff = succ->getFrequency() - (*e)->getFrequency();
                     if (comp()->getOption(TR_TraceBFGeneration))
                        traceMsg(comp(), "\t\tedge(%d->%d) frequency can be set to succ_%d(%d) - e_freq(%d->%d)(%d) = ee_freq(%d)\n",
                        ee->getFrom()->getNumber(), ee->getTo()->getNumber(), succ->getNumber(), succ->getFrequency(), (*e)->getFrom()->getNumber(), (*e)->getTo()->getNumber(),(*e)->getFrequency(), ff);
                     if (ee->getVisitCount()>1)
                        {
                        if (comp()->getOption(TR_TraceBFGeneration))
                           traceMsg(comp(), "\t\tedge frequency = %d already final\n", ee->getFrequency());
                        }
                     else
                        {
                        ee->setFrequency(ff);
                        ee->setVisitCount(2);
                        if (comp()->getOption(TR_TraceBFGeneration))
                           traceMsg(comp(), "\t\tset edge frequency = %d as final\n", ff);
                        }

                     //TODO: can continue to check the other successors of this predecessor
                     }
                  }
               }
            else
               successorFrequency = successorFrequency + normalizedFrequency;

            //dumpOptDetails("normalizedFreq %d succFreq %d node %d succ %d\n", normalizedFrequency, successorFrequency, node->getNumber(), e->getTo()->getNumber());
            }

         //if (successorFrequency > 0)
            {
            for (auto e = node->getSuccessors().begin(); e != node->getSuccessors().end(); ++e)
               {
               //if (e->getFrequency() <= 0)
                  {
                  int32_t toFrequency = (*e)->getTo()->getFrequency();
                  int32_t edgeFrequency;
                  if ((toFrequency > MAX_COLD_BLOCK_COUNT) && (frequency > MAX_COLD_BLOCK_COUNT))
                     {
                     if (successorFrequency > 0)
                        {
                        edgeFrequency = (frequency*toFrequency)/successorFrequency;
                        if (edgeFrequency <= MAX_COLD_BLOCK_COUNT)
                           edgeFrequency = MAX_COLD_BLOCK_COUNT+1;
                        }
                     else
                        edgeFrequency = 0;
                     }
                  else
                     {
                     if (toFrequency < frequency)
                        edgeFrequency = toFrequency;
                     else
                        edgeFrequency = frequency;
                     }

                  //dumpOptDetails("edgeFrequency %d frequency %d toFrequency %d succFrequency %d max %d\n", edgeFrequency, frequency, toFrequency, successorFrequency, SHRT_MAX);
                  if ((*e)->getVisitCount() > 1)
                     {
                     if (comp()->getOption(TR_TraceBFGeneration))
                        traceMsg(comp(), "\t\tedge frequency = %d is final, don't change\n", (*e)->getFrequency());
                     edgeFrequency = (*e)->getFrequency();
                     }
                  else
                     (*e)->setFrequency(edgeFrequency);

                  if (edgeFrequency > _maxEdgeFrequency)
                     _maxEdgeFrequency = edgeFrequency;
                  //dumpOptDetails("Edge %p between %d and %d has freq %d max %d\n", e, e->getFrom()->getNumber(), e->getTo()->getNumber(), e->getFrequency(), _maxEdgeFrequency);
                  }
               }
            }

         if (_externalProfiler && (node->getSuccessors().size() == 2))
            {
            bool frequencyIsIdentical = true;
            int32_t deFrq = node->getSuccessors().front()->getFrequency();
            if(deFrq != (*(++node->getSuccessors().begin()))->getFrequency())
               frequencyIsIdentical = false;
            // if both edges have same frequency then break the tie
            // using interpreter frequencies
            if (frequencyIsIdentical)
               {
               TR::Block *block = node->asBlock();
               if (!block->isCold() && block->getEntry()!=NULL && block->getLastRealTreeTop()->getNode()->getOpCode().isBranch())
                  {
                  TR::Node *ifNode = block->getLastRealTreeTop()->getNode();
                  int32_t fallThroughCount = 0;
                  int32_t branchToCount = 0;

                  _externalProfiler->getBranchCounters(ifNode, block->getNextBlock()->getEntry(), &branchToCount, &fallThroughCount, comp());

                  if (branchToCount > fallThroughCount)
                     {
                     TR::Block *branchToBlock = ifNode->getBranchDestination()->getNode()->getBlock();

                     for (auto e = node->getSuccessors().begin(); e != node->getSuccessors().end(); ++e)
                        {
                        if ((*e)->getTo()->asBlock() == branchToBlock)
                           {
                           //dumpOptDetails(comp(), "\tadjusting branch to frequency to %d in order to break the tie\n", deFrq+1);
                           (*e)->setFrequency(deFrq+1);
                           branchToBlock->setFrequency(branchToBlock->getFrequency()+1);
                           break;
                           }
                        }
                     }
                  }
               }
            }

         int32_t predecessorFrequency = 0;
         for (auto e = node->getPredecessors().begin(); e != node->getPredecessors().end(); ++e)
            {
            int32_t normalizedFrequency = (*e)->getFrom()->getFrequency();

            // DORIT: try to infer about edge frequencies
            if ((*e)->getVisitCount() > 1)
               {
               // don't add to successorFrequency sum;
               // peek at cousin blocks to see if frequency of other edges can be determined.
               // step1: if this successor has only one other predecessor...:
               TR::Block *pred = (*e)->getFrom()->asBlock();
               if (pred->getSuccessors().size() == 2) //pred has only one other successor
                  {
                  TR::CFGEdge *ee = pred->getSuccessors().front();
                  if (ee->getTo() == node)
                     ee = *(++pred->getSuccessors().begin());
                  if ((pred->getFrequency() >= 0) &&
                      (pred->getFrequency() >= (*e)->getFrequency()))
                     {
                     int32_t ff = pred->getFrequency() - (*e)->getFrequency();
                     if (comp()->getOption(TR_TraceBFGeneration))
                        traceMsg(comp(), "\t\tedge(%d->%d) frequency can be set to pred_%d(%d) - e_freq(%d->%d)(%d) = ee_freq(%d)\n",
                        ee->getFrom()->getNumber(), ee->getTo()->getNumber(), pred->getNumber(), pred->getFrequency(), (*e)->getFrom()->getNumber(), (*e)->getTo()->getNumber(), (*e)->getFrequency(), ff);

                     if (ee->getVisitCount()>1)
                        {
                        if (comp()->getOption(TR_TraceBFGeneration))
                           traceMsg(comp(), "\t\tedge frequency = %d already final\n", ee->getFrequency());
                        }
                     else
                        {
                        ee->setFrequency(ff);
                        ee->setVisitCount(2);
                        if (comp()->getOption(TR_TraceBFGeneration))
                           traceMsg(comp(), "\t\tset edge frequency = %d as final\n", ff);
                        }

                     //TODO: can continue to check the other successors of this predecessor
                     }
                  }
               }
            else
               predecessorFrequency = predecessorFrequency + normalizedFrequency;

            //dumpOptDetails("normalizedFreq %d succFreq %d node %d succ %d\n", normalizedFrequency, successorFrequency, node->getNumber(), e->getTo()->getNumber());
            }

         //if (predecessorFrequency > 0)
            {
            for (auto e = node->getPredecessors().begin(); e != node->getPredecessors().end(); ++e)
               {
               //if (e->getFrequency() <= 0)
                  {
                  int32_t fromFrequency = (*e)->getFrom()->getFrequency();
                  int32_t edgeFrequency;
                  if ((fromFrequency > MAX_COLD_BLOCK_COUNT) && (frequency > MAX_COLD_BLOCK_COUNT))
                     {
                     if (predecessorFrequency > 0)
                        {
                        edgeFrequency = (frequency*fromFrequency)/predecessorFrequency;
                        if (edgeFrequency <= MAX_COLD_BLOCK_COUNT)
                           edgeFrequency = MAX_COLD_BLOCK_COUNT+1;
                        }
                     else
                        edgeFrequency = 0;
                     }
                  else
                     {
                     if (fromFrequency < frequency)
                        edgeFrequency = fromFrequency;
                     else
                        edgeFrequency = frequency;
                     }

                  //dumpOptDetails("edgeFrequency %d frequency %d toFrequency %d succFrequency %d max %d\n", edgeFrequency, frequency, toFrequency, successorFrequency, SHRT_MAX);

                  if ((*e)->getVisitCount() > 1)
                     {
                     if (comp()->getOption(TR_TraceBFGeneration))
                        traceMsg(comp(), "\t\tedge frequency = %d is final, don't change\n", (*e)->getFrequency());
                     edgeFrequency = (*e)->getFrequency();
                     }
                  else
                     (*e)->setFrequency(edgeFrequency);

                  if (edgeFrequency > _maxEdgeFrequency)
                     _maxEdgeFrequency = edgeFrequency;
                  //dumpOptDetails("Edge %p between %d and %d has freq %d max %d\n", e, e->getFrom()->getNumber(), e->getTo()->getNumber(), e->getFrequency(), _maxEdgeFrequency);
                  }
               }
            }
         }
      else
         {
         if (frequency > origMaxFrequency)
            {
            if (!nodesToBeNormalized)
               nodesToBeNormalized = new (trStackMemory()) TR_BitVector(getNextNodeNumber(), trMemory(), stackAlloc);

            nodesToBeNormalized->set(node->getNumber());
            //dumpOptDetails(comp(),"NOT normalized %d freq %d\n", node->getNumber(), frequency);
            }
         }
      }


   nextNode = getFirstNode();
   for (; nextNode != NULL; nextNode = nextNode->getNext())
       {
       TR_SuccessorIterator sit(nextNode);
       TR::CFGEdge * edge = sit.getFirst();
       for(; edge != NULL; edge=sit.getNext())
           edge->setVisitCount(0);
       }

   return nodesToBeNormalized;
   }


static int32_t summarizeFrequencyFromPredecessors(TR::CFGNode *node, TR::CFG *cfg)
   {
   int32_t sum = 0;
   TR_PredecessorIterator pit(node);

   for (TR::CFGEdge *edge = pit.getFirst(); edge; edge = pit.getNext())
      {
      TR::CFGNode *pred = edge->getFrom();

      if (edge->getFrequency()<=0)
         continue;

      int32_t edgeFreq = edge->getFrequency();

      /*TR_BitVector *frequencySet = cfg->getFrequencySet();

      if (frequencySet)
         {
         if (!frequencySet->get(pred->getNumber()))
            {
            int32_t rawScalingFactor = cfg->getOldMaxEdgeFrequency();
            if (rawScalingFactor < 0)
               rawScalingFactor = cfg->getMaxEdgeFrequency();

            //traceMsg(comp, "raw scaling %d max %d old max %d\n", rawScalingFactor, cfg->getMaxEdgeFrequency(), cfg->getOldMaxEdgeFrequency());

            if (rawScalingFactor > 0)
               {
               if (edgeFreq > MAX_COLD_BLOCK_COUNT)
                  {
                  edgeFreq = (rawScalingFactor*edgeFreq)/(MAX_COLD_BLOCK_COUNT+MAX_BLOCK_COUNT);
                  }
               }
            }
         }      */

      sum += edgeFreq;
      }

   return sum;
   }


void
J9::CFG::setBlockFrequenciesBasedOnInterpreterProfiler()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());
   int32_t numBlocks = getNextNodeNumber();

   TR_BitVector *_seenNodes = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   TR_BitVector *_seenNodesInCycle = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _frequencySet = new (trHeapMemory()) TR_BitVector(numBlocks, trMemory(), heapAlloc, notGrowable);
   int32_t startFrequency = AVG_FREQ;
   int32_t taken          = AVG_FREQ;
   int32_t nottaken       = AVG_FREQ;
   int32_t initialCallScanFreq = -1;
   int32_t inlinedSiteIndex = -1;

   // Walk until we find if or switch statement
   TR::CFGNode *start = getStart();
   TR::CFGNode *temp  = start;
   TR_ScratchList<TR::CFGNode> upStack(trMemory());

   bool backEdgeExists = false;
   TR::TreeTop *methodScanEntry = NULL;
   //_maxFrequency = 0;
   while (  (temp->getSuccessors().size() == 1) ||
            !temp->asBlock()->getEntry()        ||
            !((temp->asBlock()->getLastRealTreeTop()->getNode()->getOpCode().isBranch()
              && !temp->asBlock()->getLastRealTreeTop()->getNode()->getByteCodeInfo().doNotProfile()) ||
             temp->asBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::lookup ||
             temp->asBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::trtLookup ||
             temp->asBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::table))
      {
      if (_seenNodes->isSet(temp->getNumber()))
         break;

      upStack.add(temp);
      _seenNodes->set(temp->getNumber());

      if (!backEdgeExists && !temp->getPredecessors().empty() && !(temp->getPredecessors().size() == 1))
         {
         if (comp()->getHCRMode() != TR::osr)
            backEdgeExists = true;
         else 
            {
            // Count the number of edges from non OSR blocks
            int32_t nonOSREdges = 0;
            for (auto edge = temp->getPredecessors().begin(); edge != temp->getPredecessors().end(); ++edge)
               {
               if ((*edge)->getFrom()->asBlock()->isOSRCodeBlock() || (*edge)->getFrom()->asBlock()->isOSRCatchBlock())
                  continue;
               if (nonOSREdges > 0)
                  {
                  backEdgeExists = true;
                  break;
                  }
               nonOSREdges++;
               }
            }
         }

      if (temp->asBlock()->getEntry() && initialCallScanFreq < 0)
         {
         int32_t methodScanFreq = scanForFrequencyOnSimpleMethod(temp->asBlock()->getEntry(), temp->asBlock()->getExit());
         if (methodScanFreq > 0)
            initialCallScanFreq = methodScanFreq;
         inlinedSiteIndex = temp->asBlock()->getEntry()->getNode()->getInlinedSiteIndex();
         methodScanEntry = temp->asBlock()->getEntry();
         }

      if (!temp->getSuccessors().empty())
         {
         if ((temp->getSuccessors().size() == 2) && temp->asBlock() && temp->asBlock()->getNextBlock())
            {
            temp = temp->asBlock()->getNextBlock();
            }
         else
            temp = temp->getSuccessors().front()->getTo();
         }
      else
         break;
      }

   if (comp()->getOption(TR_TraceBFGeneration))
      dumpOptDetails(comp(),"Propagation start block_%d\n", temp->getNumber());

   if (temp->asBlock()->getEntry())
      inlinedSiteIndex = temp->asBlock()->getEntry()->getNode()->getInlinedSiteIndex();

   if ((temp->getSuccessors().size() == 2) &&
       temp->asBlock()->getEntry()         &&
       temp->asBlock()->getLastRealTreeTop()->getNode()->getOpCode().isBranch() &&
       !temp->asBlock()->getLastRealTreeTop()->getNode()->getByteCodeInfo().doNotProfile())
      {
      getInterpreterProfilerBranchCountersOnDoubleton(temp, &taken, &nottaken);
      startFrequency = taken + nottaken;
      self()->setEdgeFrequenciesOnNode( temp, taken, nottaken, comp());
      }
   else if (temp->asBlock()->getEntry() &&
            (temp->asBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::lookup ||
             temp->asBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::trtLookup ||
             temp->asBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::table))
      {
      startFrequency = _externalProfiler->getSumSwitchCount(temp->asBlock()->getLastRealTreeTop()->getNode(), comp());
      if (comp()->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp(),"Switch with total frequency of %d\n", startFrequency);
      setSwitchEdgeFrequenciesOnNode(temp, comp());
      }
   else
      {
      if (_calledFrequency > 0)
         startFrequency = _calledFrequency;
      else if (initialCallScanFreq > 0)
         startFrequency = initialCallScanFreq;
      else
         {
         startFrequency = AVG_FREQ;
         }

      if ((temp->getSuccessors().size() == 2) && (startFrequency > 0) && temp->asBlock()->getEntry() && temp->asBlock()->getLastRealTreeTop()->getNode()->getOpCode().isBranch())
         self()->setEdgeFrequenciesOnNode( temp, 0, startFrequency, comp());
      else
         self()->setUniformEdgeFrequenciesOnNode(temp, startFrequency, false, comp());
      }

   setBlockFrequency (temp, startFrequency);
   _initialBlockFrequency = startFrequency;

   if (comp()->getOption(TR_TraceBFGeneration))
      dumpOptDetails(comp(),"Set frequency of %d on block_%d\n", temp->asBlock()->getFrequency(), temp->getNumber());

   start = temp;

   // Walk backwards to the start and propagate this frequency

   if (comp()->getOption(TR_TraceBFGeneration))
      dumpOptDetails(comp(),"Propagating start frequency backwards...\n");

   ListIterator<TR::CFGNode> upit(&upStack);
   for (temp = upit.getFirst(); temp; temp = upit.getNext())
      {
      if (!temp->asBlock()->getEntry())
         continue;
      if ((temp->getSuccessors().size() == 2) && (startFrequency > 0) && temp->asBlock()->getEntry() && temp->asBlock()->getLastRealTreeTop()->getNode()->getOpCode().isBranch())
         self()->setEdgeFrequenciesOnNode( temp, 0, startFrequency, comp());
      else
         self()->setUniformEdgeFrequenciesOnNode( temp, startFrequency, false, comp());
      setBlockFrequency (temp, startFrequency);
      _seenNodes->set(temp->getNumber());

      if (comp()->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp(),"Set frequency of %d on block_%d\n", temp->asBlock()->getFrequency(), temp->getNumber());
      }

   if (comp()->getOption(TR_TraceBFGeneration))
      dumpOptDetails(comp(),"Propagating block frequency forward...\n");

   // Walk reverse post-order
   // we start at the first if or switch statement
   TR_ScratchList<TR::CFGNode> stack(trMemory());
   stack.add(start);

   while (!stack.isEmpty())
      {
      TR::CFGNode *node = stack.popHead();

      if (comp()->getOption(TR_TraceBFGeneration))
         traceMsg(comp(), "Considering block_%d\n", node->getNumber());

      if (_seenNodes->isSet(node->getNumber()))
         continue;

      _seenNodes->set(node->getNumber());

      if (!node->asBlock()->getEntry())
         continue;

      inlinedSiteIndex = node->asBlock()->getEntry()->getNode()->getInlinedSiteIndex();

      if (node->asBlock()->isCold())
         {
         if (comp()->getOption(TR_TraceBFGeneration))
            dumpOptDetails(comp(),"Analyzing COLD block_%d\n", node->getNumber());
         //node->asBlock()->setFrequency(0);
         int32_t freq = node->getFrequency();
         if ((node->getSuccessors().size() == 2) && (freq > 0) && node->asBlock()->getEntry() && node->asBlock()->getLastRealTreeTop()->getNode()->getOpCode().isBranch())
            self()->setEdgeFrequenciesOnNode( node, 0, freq, comp());
         else
            self()->setUniformEdgeFrequenciesOnNode(node, freq, false, comp());
         setBlockFrequency (node, freq);
         //continue;
         }

      // ignore the first node
      if (!node->asBlock()->isCold() && (node != start))
         {
         if ((node->getSuccessors().size() == 2) &&
             node->asBlock()->getLastRealTreeTop()->getNode()->getOpCode().isBranch())
            {
            // if it's an if and it's not JIT inserted artificial if then just
            // read the outgoing branches and put that as node frequency
            // else get frequency from predecessors
            if (!isVirtualGuard(node->asBlock()->getLastRealTreeTop()->getNode()) &&
                !node->asBlock()->getLastRealTreeTop()->getNode()->getByteCodeInfo().doNotProfile())
               {
               _seenNodesInCycle->empty();
               getInterpreterProfilerBranchCountersOnDoubleton(node, &taken, &nottaken);

               if ((taken <= 0) && (nottaken <= 0))
                  {
                  taken = LOW_FREQ;
                  nottaken = LOW_FREQ;
                  }

               self()->setEdgeFrequenciesOnNode( node, taken, nottaken, comp());
               setBlockFrequency (node, taken + nottaken);

               if (comp()->getOption(TR_TraceBFGeneration))
                  dumpOptDetails(comp(),"If on node %p is not guard I'm using the taken and nottaken counts for producing block frequency\n", node->asBlock()->getLastRealTreeTop()->getNode());
               }
            else
               {
               if (comp()->getOption(TR_TraceBFGeneration))
                  {
                  TR_PredecessorIterator pit(node);
                  for (TR::CFGEdge *edge = pit.getFirst(); edge; edge = pit.getNext())
                     {
                     TR::CFGNode *pred = edge->getFrom();

                     if (edge->getFrequency()<=0)
                        continue;

                     int32_t edgeFreq = edge->getFrequency();

                     if (comp()->getOption(TR_TraceBFGeneration))
                        traceMsg(comp(), "11Pred %d has freq %d\n", pred->getNumber(), edgeFreq);
                     }
                  }

               TR::CFGNode *predNotSet = NULL;
               TR_PredecessorIterator pit(node);
               for (TR::CFGEdge *edge = pit.getFirst(); edge; edge = pit.getNext())
                  {
                  TR::CFGNode *pred = edge->getFrom();

                  if (pred->getFrequency()< 0)
                     {
                     predNotSet = pred;
                     break;
                     }
                  }

               if (predNotSet &&
                   !_seenNodesInCycle->get(node->getNumber()))
                  {
                  stack.add(predNotSet);
                  _seenNodesInCycle->set(node->getNumber());
                  _seenNodes->reset(node->getNumber());
                  continue;
                  }

               if (!predNotSet)
                  _seenNodesInCycle->empty();

               int32_t sumFreq = summarizeFrequencyFromPredecessors(node, self());
               if (sumFreq <= 0)
                  sumFreq = AVG_FREQ;
               self()->setEdgeFrequenciesOnNode( node, 0, sumFreq, comp());
               setBlockFrequency (node, sumFreq);

               if (comp()->getOption(TR_TraceBFGeneration))
                  dumpOptDetails(comp(),"If on node %p is guard I'm using the predecessor frequency sum\n", node->asBlock()->getLastRealTreeTop()->getNode());
               }

            if (comp()->getOption(TR_TraceBFGeneration))
               dumpOptDetails(comp(),"Set frequency of %d on block_%d\n", node->asBlock()->getFrequency(), node->getNumber());
            }
         else if (node->asBlock()->getEntry() &&
                  (node->asBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::lookup ||
                   node->asBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::trtLookup ||
                   node->asBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::table))
            {
            _seenNodesInCycle->empty();
            int32_t sumFreq = _externalProfiler->getSumSwitchCount(node->asBlock()->getLastRealTreeTop()->getNode(), comp());
            setSwitchEdgeFrequenciesOnNode(node, comp());
            setBlockFrequency (node, sumFreq);
            if (comp()->getOption(TR_TraceBFGeneration))
               {
               dumpOptDetails(comp(),"Found a Switch statement at exit of block_%d\n", node->getNumber());
               dumpOptDetails(comp(),"Set frequency of %d on block_%d\n", node->asBlock()->getFrequency(), node->getNumber());
               }
            }
         else
            {
            TR::CFGNode *predNotSet = NULL;
            int32_t sumFreq = 0;
            TR_PredecessorIterator pit(node);
            for (TR::CFGEdge *edge = pit.getFirst(); edge; edge = pit.getNext())
               {
               TR::CFGNode *pred = edge->getFrom();

               if (pred->getFrequency()< 0)
                  {
                  predNotSet = pred;
                  break;
                  }

                int32_t edgeFreq = edge->getFrequency();
                sumFreq += edgeFreq;
        }

            if (predNotSet &&
                !_seenNodesInCycle->get(node->getNumber()))
               {
               _seenNodesInCycle->set(node->getNumber());
               _seenNodes->reset(node->getNumber());
               stack.add(predNotSet);
               continue;
               }
            else
               {
               if (!predNotSet)
                  _seenNodesInCycle->empty();

               if (comp()->getOption(TR_TraceBFGeneration))
                  traceMsg(comp(), "2Setting block and uniform freqs\n");

              if ((node->getSuccessors().size() == 2) && (sumFreq > 0) && node->asBlock()->getEntry() && node->asBlock()->getLastRealTreeTop()->getNode()->getOpCode().isBranch())
                 self()->setEdgeFrequenciesOnNode( node, 0, sumFreq, comp());
              else
                 self()->setUniformEdgeFrequenciesOnNode( node, sumFreq, false, comp());
               setBlockFrequency (node, sumFreq);
               }

            if (comp()->getOption(TR_TraceBFGeneration))
               {
               dumpOptDetails(comp(),"Not an if (or unknown if) at exit of block %d (isSingleton=%d)\n", node->getNumber(), (node->getSuccessors().size() == 1));
               dumpOptDetails(comp(),"Set frequency of %d on block %d\n", node->asBlock()->getFrequency(), node->getNumber());
               }
            }
         }

      TR_SuccessorIterator sit(node);
      // add the successors on the list
      for (TR::CFGEdge *edge = sit.getFirst(); edge; edge = sit.getNext())
         {
         TR::CFGNode *succ = edge->getTo();

         if (!_seenNodes->isSet(succ->getNumber()))
            stack.add(succ);
         else
            {
            if (!(((succ->getSuccessors().size() == 2) &&
                 succ->asBlock()->getLastRealTreeTop()->getNode()->getOpCode().isBranch()) ||
                (succ->asBlock()->getEntry() &&
                  (succ->asBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::lookup ||
                   succ->asBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::trtLookup ||
                   succ->asBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::table))))
               {
               setBlockFrequency ( succ, edge->getFrequency(), true);

               // addup this edge to the frequency of the blocks following it
               // propagate downward until you reach a block that doesn't end in
               // goto or cycle
               if (succ->getSuccessors().size() == 1)
                  {
                  TR::CFGNode *tempNode = succ->getSuccessors().front()->getTo();
                  TR_BitVector *_seenGotoNodes = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
                   _seenGotoNodes->set(succ->getNumber());
                  while ((tempNode->getSuccessors().size() == 1) &&
                         (tempNode != succ) &&
                         (tempNode != getEnd()) &&
                         !_seenGotoNodes->isSet(tempNode->getNumber()))
                     {
                     TR::CFGNode *nextTempNode = tempNode->getSuccessors().front()->getTo();

                     if (comp()->getOption(TR_TraceBFGeneration))
                        traceMsg(comp(), "3Setting block and uniform freqs\n");

                     self()->setUniformEdgeFrequenciesOnNode( tempNode, edge->getFrequency(), true, comp());
                     setBlockFrequency (tempNode, edge->getFrequency(), true);

                     _seenGotoNodes->set(tempNode->getNumber());
                     tempNode = nextTempNode;
                     }
                  }
               }
            }
         }
      }


   if (backEdgeExists)
      {
      while (!upStack.isEmpty())
         {
         TR::CFGNode *temp = upStack.popHead();
         if (temp == start)
            continue;

         if (backEdgeExists)
            temp->setFrequency(std::max(MAX_COLD_BLOCK_COUNT+1, startFrequency/10));
         }
      }


   // now set all the blocks that haven't been seen to have 0 frequency
   // catch blocks are one example, we don't try to set frequency there to
   // save compilation time, since those are most frequently cold. Future might prove
   // this otherwise.

   TR::CFGNode *node;
   start = getStart();
   for (node = getFirstNode(); node; node=node->getNext())
      {
      if ((node == getEnd()) || (node == start))
          node->asBlock()->setFrequency(0);

      if (_seenNodes->isSet(node->getNumber()))
         continue;

      if (node->asBlock()->getEntry() &&
         (node->asBlock()->getFrequency()<0))
         node->asBlock()->setFrequency(0);
      }

   //scaleEdgeFrequencies();

   TR_BitVector *nodesToBeNormalized = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   nodesToBeNormalized->setAll(numBlocks);
   _maxFrequency = -1;
   _maxEdgeFrequency = -1;
   normalizeFrequencies(nodesToBeNormalized);
   _oldMaxFrequency = _maxFrequency;
   _oldMaxEdgeFrequency = _maxEdgeFrequency;

   if (comp()->getOption(TR_TraceBFGeneration))
      traceMsg(comp(), "max freq %d max edge freq %d\n", _maxFrequency, _maxEdgeFrequency);

   }


void
J9::CFG::computeInitialBlockFrequencyBasedOnExternalProfiler(TR::Compilation *comp)
   {
   TR_ExternalProfiler* profiler = comp->fej9()->hasIProfilerBlockFrequencyInfo(*comp);
   if (!profiler)
      {
      _initialBlockFrequency = AVG_FREQ;
      return;
      }

   _externalProfiler = profiler;

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());
   int32_t numBlocks = getNextNodeNumber();

   TR_BitVector *_seenNodes = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _frequencySet = new (trHeapMemory()) TR_BitVector(numBlocks, trMemory(), heapAlloc, notGrowable);
   int32_t startFrequency = AVG_FREQ;
   int32_t taken          = AVG_FREQ;
   int32_t nottaken       = AVG_FREQ;
   int32_t initialCallScanFreq = -1;
   int32_t inlinedSiteIndex = -1;

   // Walk until we find if or switch statement
   TR::CFGNode *start = getStart();
   TR::CFGNode *temp  = start;

   bool backEdgeExists = false;
   TR::TreeTop *methodScanEntry = NULL;

   while (  (temp->getSuccessors().size() == 1) ||
            !temp->asBlock()->getEntry()        ||
            !((temp->asBlock()->getLastRealTreeTop()->getNode()->getOpCode().isBranch()
              && !temp->asBlock()->getLastRealTreeTop()->getNode()->getByteCodeInfo().doNotProfile()) ||
             temp->asBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::lookup ||
             temp->asBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::trtLookup ||
             temp->asBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::table))
      {
      if (_seenNodes->isSet(temp->getNumber()))
         break;

      _seenNodes->set(temp->getNumber());

      if (!temp->getPredecessors().empty() && !(temp->getPredecessors().size() == 1))
         backEdgeExists = true;

      if (temp->asBlock()->getEntry() && initialCallScanFreq < 0)
         {
         int32_t methodScanFreq = scanForFrequencyOnSimpleMethod(temp->asBlock()->getEntry(), temp->asBlock()->getExit());
         if (methodScanFreq > 0)
            initialCallScanFreq = methodScanFreq;
         inlinedSiteIndex = temp->asBlock()->getEntry()->getNode()->getInlinedSiteIndex();
         methodScanEntry = temp->asBlock()->getEntry();
         }

      if (!temp->getSuccessors().empty())
         {
         if ((temp->getSuccessors().size() == 2) && temp->asBlock() && temp->asBlock()->getNextBlock())
            {
            temp = temp->asBlock()->getNextBlock();
            }
         else
            temp = temp->getSuccessors().front()->getTo();
         }
      else
         break;
      }

   if (temp->asBlock()->getEntry())
      inlinedSiteIndex = temp->asBlock()->getEntry()->getNode()->getInlinedSiteIndex();

   if ((temp->getSuccessors().size() == 2) &&
       temp->asBlock()->getEntry()         &&
       temp->asBlock()->getLastRealTreeTop()->getNode()->getOpCode().isBranch() &&
       !temp->asBlock()->getLastRealTreeTop()->getNode()->getByteCodeInfo().doNotProfile())
      {
      getInterpreterProfilerBranchCountersOnDoubleton(temp, &taken, &nottaken);
      if ((taken <= 0) && (nottaken <= 0))
         {
         taken = LOW_FREQ;
         nottaken = LOW_FREQ;
         }
      startFrequency = taken + nottaken;
      }
   else if (temp->asBlock()->getEntry() &&
            (temp->asBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::lookup ||
             temp->asBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::trtLookup ||
             temp->asBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::table))
      {
      startFrequency = _externalProfiler->getSumSwitchCount(temp->asBlock()->getLastRealTreeTop()->getNode(), comp);
      }
   else
      {
      if (_calledFrequency > 0)
         startFrequency = _calledFrequency;
      else if (initialCallScanFreq > 0)
         startFrequency = initialCallScanFreq;
      else
         {
         startFrequency = AVG_FREQ;
         }
      }

   if (startFrequency <= 0)
      startFrequency = LOW_FREQ;

   _initialBlockFrequency = startFrequency;
   }


static int32_t
getParentCallCount(TR::CFG *cfg, TR::Node *node)
   {
   if (node->getByteCodeInfo().getCallerIndex() >=-1)
      {
      int32_t parentSiteIndex = node->getInlinedSiteIndex();

      if (parentSiteIndex >= 0)
         {
         TR_InlinedCallSite & site = cfg->comp()->getInlinedCallSite(parentSiteIndex);
         int32_t callCount = cfg->comp()->fej9()->getIProfilerCallCount(site._byteCodeInfo, cfg->comp());

         if (callCount != 0)
            return callCount;
         }
      }
   else
      { // It's a dummy block in estimate code size
        // The called frequency is set by estimate code size because at that time
        // we don't have the final caller information.
      int32_t callCount = cfg->_calledFrequency;

      if (callCount != 0)
         return callCount;
      }

   return 0;
   }


void
J9::CFG::getInterpreterProfilerBranchCountersOnDoubleton(TR::CFGNode *cfgNode, int32_t *taken, int32_t *nottaken)
   {
   TR::Node *node = cfgNode->asBlock()->getLastRealTreeTop()->getNode();

   if (this != comp()->getFlowGraph())
      {
      TR::TreeTop *entry = (cfgNode->asBlock()->getNextBlock()) ? cfgNode->asBlock()->getNextBlock()->getEntry() : NULL;
      _externalProfiler->getBranchCounters(node, entry, taken, nottaken, comp());
      }
   else
      {
      getBranchCounters(node, cfgNode->asBlock(), taken, nottaken, comp());
      }

   if (*taken || *nottaken)
      {
      if (comp()->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp(),"If on node %p has branch counts: taken=%d, not taken=%d\n", node, *taken, *nottaken);
      }
   else if (isVirtualGuard(node))
      {
      *taken = 0;
      *nottaken = AVG_FREQ;
      int32_t sumFreq = summarizeFrequencyFromPredecessors(cfgNode, self());

      if (sumFreq>0)
         *nottaken = sumFreq;

      if (comp()->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp(),"Guard on node %p has default branch counts: taken=%d, not taken=%d\n", node, *taken, *nottaken);
      }
   else if (!cfgNode->asBlock()->isCold())
      {
      if (node->getBranchDestination()->getNode()->getBlock()->isCold())
         *taken = 0;
      else
         *taken = LOW_FREQ;

      if (cfgNode->asBlock()->getNextBlock() &&
          cfgNode->asBlock()->getNextBlock()->isCold())
         *nottaken = 0;
      else
         *nottaken = LOW_FREQ;

      /*int32_t sumFreq = summarizeFrequencyFromPredecessors(cfgNode, this);

      if (sumFreq>0)
         {
         *nottaken = sumFreq>>1;
         *taken = *nottaken;
         }
      else
         {
         if (node->getByteCodeIndex()==0)
            {
            int32_t callCount = getParentCallCount(this, node);

            // we don't know what the call count is, assign some
            // moderate frequency
            if (callCount<=0)
               callCount = AVG_FREQ;

            *taken = 0;
            *nottaken = callCount;
            }
         }
      */
      if (comp()->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp(),"If with no profiling information on node %p has low branch counts: taken=%d, not taken=%d\n", node, *taken, *nottaken);
      }
   }


TR::CFGEdge * getCFGEdgeForNode(TR::CFGNode *node, TR::Node *child)
   {
   for (auto e = node->getSuccessors().begin(); e != node->getSuccessors().end(); ++e)
      {
      if ((*e)->getTo()->asBlock() == child->getBranchDestination()->getNode()->getBlock())
         return *e;
      }

   return NULL;
   }


void
J9::CFG::setSwitchEdgeFrequenciesOnNode(TR::CFGNode *node, TR::Compilation *comp)
   {
   TR::Block *block = node->asBlock();
   TR::Node *treeNode = node->asBlock()->getLastRealTreeTop()->getNode();
   int32_t sumFrequency = _externalProfiler->getSumSwitchCount(treeNode, comp);

   if (sumFrequency < 10)
      {
      if (comp->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp,"Low count switch I'll set frequencies using uniform edge distribution\n");

      self()->setUniformEdgeFrequenciesOnNode (node, sumFrequency, false, comp);
      return;
      }

   if (treeNode->getInlinedSiteIndex() < -1)
      {
      if (comp->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp,"Dummy switch generated in estimate code size I'll set frequencies using uniform edge distribution\n");

      self()->setUniformEdgeFrequenciesOnNode (node, sumFrequency, false, comp);
      return;
      }

   if (_externalProfiler->isSwitchProfileFlat(treeNode, comp))
      {
      if (comp->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp,"Flat profile switch, setting average frequency on each case.\n");

      self()->setUniformEdgeFrequenciesOnNode(node, _externalProfiler->getFlatSwitchProfileCounts(treeNode, comp), false, comp);
      return;
      }

   for ( int32_t count=1; count < treeNode->getNumChildren(); count++)
      {
      TR::Node *child = treeNode->getChild(count);
      TR::CFGEdge *e = getCFGEdgeForNode(node, child);

      int32_t frequency = _externalProfiler->getSwitchCountForValue (treeNode, (count-1), comp);
      e->setFrequency( std::max(frequency,1) );
      if (comp->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp,"Edge %p between %d and %d has freq %d (Switch)\n", e, e->getFrom()->getNumber(), e->getTo()->getNumber(), e->getFrequency());
      }
   }


void
J9::CFG::setBlockFrequency(TR::CFGNode *node, int32_t frequency, bool addFrequency)
   {
   TR::Block *block = node->asBlock();
   if (!block)
      return;

   if (block->isCold())
      {
      if (comp()->getOption(TR_TraceBFGeneration))
         {
         traceMsg(
            comp(),
            "Leaving cold reason %d on block_%d\n",
            block->getFrequency(),
            block->getNumber());
         }
      return;
      }

   if (comp()->getOption(TR_TraceBFGeneration))
      traceMsg(comp(), "Original freq %d on block_%d incoming freq %d\n", block->getFrequency(), block->getNumber(), frequency);

   if (_frequencySet)
      {
      if (!_frequencySet->get(block->getNumber()))
         {
         _frequencySet->set(block->getNumber());
         if (comp()->getOption(TR_TraceBFGeneration))
            traceMsg(comp(), "00 Setting freq %d on block_%d added freq %d\n", block->getFrequency(), block->getNumber(), 0);
         block->setFrequency(0);
         }
      }

   if ((block->getFrequency() < 0) || block->isCatchBlock())
      addFrequency = false;

   if (addFrequency)
      {
      int32_t addedFrequency = block->getFrequency() + frequency;
      //if (addedFrequency > _maxFrequency)
      //   _maxFrequency = addedFrequency;

      block->setFrequency(addedFrequency);
      if (comp()->getOption(TR_TraceBFGeneration))
         traceMsg(comp(), "11 Setting freq %d on block_%d added freq %d\n", block->getFrequency(), block->getNumber(), addedFrequency);
      }
   else
      {
      //if (frequency > _maxFrequency)
      //   _maxFrequency = frequency;

      block->setFrequency(frequency);
      if (comp()->getOption(TR_TraceBFGeneration))
         traceMsg(comp(), "22 Setting freq %d on block_%d\n", block->getFrequency(), block->getNumber());
      }
   return;
   }


// We currently don't handle well straight line methods without branches
// when using interpreter profiler. If there are no branches or calls
// we don't have a frequency to set. The function below tries to find any
// possible interpreter profiling information to continue on.
int32_t
J9::CFG::scanForFrequencyOnSimpleMethod(TR::TreeTop *tt, TR::TreeTop *endTT)
   {
   if (comp()->getOption(TR_TraceBFGeneration))
      traceMsg(comp(), "Starting method scan...\n");
   for (; tt && tt!=endTT; tt = tt->getNextTreeTop())
      {
      if (!tt->getNode()) continue;

      TR::Node *node = tt->getNode();

      if (node->getOpCode().isTreeTop() && node->getNumChildren()>0 && node->getFirstChild()->getOpCode().isCall())
         node = node->getFirstChild();

      if (comp()->getOption(TR_TraceBFGeneration))
         traceMsg(comp(), "Scanning node %p, isBranch = %d, isCall = %d, isVirtualCall =%d\n",
            node, node->getOpCode().isBranch(),
            node->getOpCode().isCall(), node->getOpCode().isCallIndirect());

      if (node->getOpCode().isBranch()) return -1;

      if (node->getOpCode().isCallIndirect())
         {
         int32_t newFrequency = comp()->fej9()->getIProfilerCallCount(node->getByteCodeInfo(), comp());

         if (newFrequency >0)
            {
            if (comp()->getOption(TR_TraceBFGeneration))
               traceMsg(comp(), "Method scan found frequency %d\n", newFrequency);
            return newFrequency;
            }
         }
      }

   return -1;
   }


void
J9::CFG::getBranchCountersFromProfilingData(TR::Node *node, TR::Block *block, int32_t *taken, int32_t *notTaken)
   {
   TR::Compilation *comp = self()->comp();
   TR::Block *branchToBlock = node->getBranchDestination()->getNode()->getBlock();
   TR::Block *fallThroughBlock = block->getNextBlock();

   if (self() != comp->getFlowGraph())
      _externalProfiler->getBranchCounters(node, fallThroughBlock->getEntry(), taken, notTaken, comp);
   else
      {
      TR_BranchProfileInfoManager * branchManager = TR_BranchProfileInfoManager::get(comp);
      branchManager->getBranchCounters(node, fallThroughBlock->getEntry(), taken, notTaken, comp);

      bool Confirmation = comp->getOption(TR_EnableScorchInterpBlockFrequencyProfiling);

      if (Confirmation && ((comp->hasBlockFrequencyInfo() && (self() == comp->getFlowGraph())) /*|| hasJProfilingInfo(comp, self())*/))
         {
         TR_PersistentProfileInfo *profileInfo = getProfilingInfoForCFG(comp, self());
         if (profileInfo)
            {
            TR_BlockFrequencyInfo *blockFrequencyInfo = profileInfo->getBlockFrequencyInfo();
            // This check ensures that the JIT profiling block frequency can be properly correlated to interpreter profiling edge frequency
            // This can be only done when no other edge may come in to a block and unnaturally inflate one of the block's counts.
            //
            if((fallThroughBlock->getPredecessors().size() == 1) && (branchToBlock->getPredecessors().size() == 1))
               {
               int32_t currentBlockFreq = blockFrequencyInfo->getFrequencyInfo(block, comp);
               int32_t fallthruBlockFreq = blockFrequencyInfo->getFrequencyInfo(fallThroughBlock, comp);
               int32_t branchBlockFreq = blockFrequencyInfo->getFrequencyInfo(branchToBlock, comp);

               if(currentBlockFreq > 0 && fallthruBlockFreq > 0 && branchBlockFreq > 0)
                  {
                  if( (*taken > *notTaken && fallthruBlockFreq > branchBlockFreq ) || (*notTaken > *taken && branchBlockFreq > fallthruBlockFreq) )
                     {
                     if (comp->getOption(TR_TraceBFGeneration))
                        traceMsg(comp, "For block %d fallthru block %d and branch block %d  iprofiler says taken = %d notTaken = %d jitprofiler says currentBlockfreq = %d "
                           "taken = %d notTaken = %d. Scaling iprofiler info.\n",block->getNumber(),fallThroughBlock->getNumber(),branchToBlock->getNumber(),*taken,*notTaken,
                            currentBlockFreq,branchBlockFreq,fallthruBlockFreq);
                     *taken =  ((*taken) * fallthruBlockFreq ) / (fallthruBlockFreq + branchBlockFreq) ;
                     *notTaken = ((*notTaken) * branchBlockFreq) /  (fallthruBlockFreq + branchBlockFreq) ;
                     if (comp->getOption(TR_TraceBFGeneration))
                        traceMsg(comp,"New taken = %d notTaken = %d\n",*taken,*notTaken);

                     }
                  }
               }
            }
         }
      }

   }

// OMR TODO:
// This only seems necessary to NULL out the _externalProfiler field.
// Consider eliminating this project specialization altogether.
//
void
J9::CFG::setBlockAndEdgeFrequenciesBasedOnStructure()
   {
   self()->propagateFrequencyInfoFromExternalProfiler(NULL);
   }


void
J9::CFG::propagateFrequencyInfoFromExternalProfiler(TR_ExternalProfiler *profiler)
   {
   _externalProfiler = profiler;

   if (profiler)
      {
      self()->setBlockFrequenciesBasedOnInterpreterProfiler();
      return;
      }

   // Call super class
   //
   OMR::CFGConnector::setBlockAndEdgeFrequenciesBasedOnStructure();
   }


bool
J9::CFG::emitVerbosePseudoRandomFrequencies()
   {
   TR::CFGNode *nextNode = getFirstNode();
   int32_t count = 1;
   int32_t edgeIndex = 0;
   comp()->fej9()->emitNewPseudoRandomNumberVerbosePrefix();
   for (; nextNode != NULL; nextNode = nextNode->getNext())
      {
      comp()->fej9()->emitNewPseudoRandomNumberVerbose(nextNode->getFrequency());
      TR_SuccessorIterator sit(nextNode);
      for (TR::CFGEdge * nextEdge = sit.getFirst(); nextEdge != NULL; nextEdge = sit.getNext())
         {
         comp()->fej9()->emitNewPseudoRandomNumberVerbose(nextEdge->getFrequency());
         if (((count++) % 50) == 0)
            {
            comp()->fej9()->emitNewPseudoRandomVerboseSuffix();
            comp()->fej9()->emitNewPseudoRandomNumberVerbosePrefix();
            }

         edgeIndex++;
         }

      if (((count++) % 50) == 0)
         {
         comp()->fej9()->emitNewPseudoRandomVerboseSuffix();
         comp()->fej9()->emitNewPseudoRandomNumberVerbosePrefix();
         }
      }

   _numEdges = edgeIndex;

    comp()->fej9()->emitNewPseudoRandomNumberVerbose(getMaxFrequency());

    if (((count++) % 50) == 0)
       {
       comp()->fej9()->emitNewPseudoRandomVerboseSuffix();
       comp()->fej9()->emitNewPseudoRandomNumberVerbosePrefix();
       }

   comp()->fej9()->emitNewPseudoRandomVerboseSuffix();
   return true;
   }
