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

#include "optimizer/ProfileGenerator.hpp"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/TransformUtil.hpp"

#define OPT_DETAILS "O^O PROFILE GENERATOR: "
#define MAX_NUMBER_OF_ALLOWED_NODES 90000

#ifdef DEBUG
// Define range of asyncs to become split points, for debugging.
// Split point 0 is the method entry.
//
#define FIRST_SPLIT_POINT (0)
#define LAST_SPLIT_POINT  (INT_MAX)

static int32_t splitPoint; // For debugging
#endif

TR_ProfileGenerator::TR_ProfileGenerator(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}

int32_t TR_ProfileGenerator::perform()
   {
   /**
    * Duplication is not necessary under JProfiling
    */
   if (comp()->getProfilingMode() != JitProfiling)
      {
      if (trace())
         traceMsg(comp(), "Profile Generator is only required by JitProfiling instrumentation\n");
      return 0;
      }

   _asyncTree = NULL;
   int32_t nodeCount = comp()->getNodeCount();

   // We will throw an exception and start interpreting
   // if there are too many nodes; avoid cloning the method in this
   // case; also remove the profiling trees from this method as
   // we won't be actually profiling in this case
   //
   if (nodeCount > MAX_NUMBER_OF_ALLOWED_NODES)
      {
      vcount_t visitCount = comp()->incVisitCount();
      int32_t actualNodeCount = 0;
      TR::TreeTop *treeTop;
      for (treeTop=comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
         actualNodeCount = actualNodeCount + treeTop->getNode()->countNumberOfNodesInSubtree(visitCount);

      bool switchAwayFromProfilingForHotAndVeryhot = false;
      if (!comp()->getOption(TR_DisableSwitchAwayFromProfilingForHotAndVeryhot))
         switchAwayFromProfilingForHotAndVeryhot = (actualNodeCount + actualNodeCount) > comp()->getOptions()->getProfilingCompNodecountThreshold()
                                                    && (comp()->getMethodHotness() == hot
                                                        || (comp()->getMethodHotness() == veryHot
                                                            && comp()->isCpuExpensiveCompilation((int64_t)1000000 * comp()->getOptions()->getCpuCompTimeExpensiveThreshold()) == TR_yes));
      if (!comp()->getOption(TR_ProcessHugeMethods) && (((actualNodeCount + actualNodeCount) > USHRT_MAX)

          // we are avoiding cloning in more cases conservatively for profiling hot, only when double the actual node count is larger than a threshold
          // this is even more conservative for profiling very hot, where we also consider the actual CPU time spent in compilation.
          // the conditions below may be simplified further by only consider CPU time is expensive or not.
                                                         || switchAwayFromProfilingForHotAndVeryhot))

         {
         if (comp()->getMethodHotness() != hot && comp()->getMethodHotness() != veryHot && !switchAwayFromProfilingForHotAndVeryhot)
            TR_ASSERT(0, "Too many nodes in method before cloning in ProfileGenerator\n");
         TR::TreeTop *treeTop;
         TR::Block *block = NULL;
         for (treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
            {
            TR::Node *node = treeTop->getNode();
            if (node->getOpCodeValue() == TR::BBStart)
               block = node->getBlock();

            if (node->isProfilingCode())
               {
               if (node->getFirstChild()->getOpCode().isCall())
                  {
                  // For value profiling call nodes we cannot simply unlink the tree because we hold on to a reference of
                  // the value node we are to profile. As such unlinking this treetop could cause the potentially commoned
                  // value node to swing down to its next use which can be functionally incorrect. Instead we eliminate
                  // the value profiling call and we anchor the value node to a tree top and let dead tree elimination
                  // clean it up.

                  TR::Node *firstChild = node->getFirstChild()->getFirstChild();
                  firstChild->incReferenceCount();
                  node->getFirstChild()->recursivelyDecReferenceCount();
                  node->setFirst(firstChild);
                  
                  TR::Node::recreate(node, TR::treetop);
                  }
               else if (node->getOpCode().isStore())
                  {
                  // All block counters (which are stores to static symrefs) can be simply unlinked
                  treeTop->unlink(true);
                  }
               else
                  {
                  TR_ASSERT_SAFE_FATAL(false, "Unexpected node marked with isProfilingCode.");
                  }

              requestOpt(OMR::deadTreesElimination, true, block);
              }
            }

         // If profiling trees are removed, we must also switch away from profiling
         TR_ASSERT(comp()->getRecompilationInfo(), "recompilationInfo must exist because profiling compilations are followed by recompilations");
         comp()->getRecompilationInfo()->switchAwayFromProfiling();
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
            TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,"%s Switch away from profiling.", comp()->signature());
         // Some sort of message about this strange condition would be nice
         return 0;
         }
      }

   bool inColdBlock = false;

   TR::Recompilation *recompilationInfo   = comp()->getRecompilationInfo();
   TR_PersistentProfileInfo *profileInfo = TR_PersistentProfileInfo::getCurrent(comp());
   if (profileInfo &&
       profileInfo->getProfilingFrequency() == DEFAULT_PROFILING_FREQUENCY &&
       profileInfo->getMaxCount()           == DEFAULT_PROFILING_COUNT)
      {
      if (comp()->getOption(TR_QuickProfile))
         {
         profileInfo->setProfilingFrequency(2);
         profileInfo->setProfilingCount    (100);
         }
      else
         {
         int32_t numBackEdges = comp()->getMethodSymbol()->getNumberOfBackEdges();
         if (numBackEdges > MAX_BACKEDGES)
            numBackEdges = MAX_BACKEDGES;

         if (comp()->getOptions()->getProfilingFrequency() != DEFAULT_PROFILING_FREQUENCY)
            profileInfo->setProfilingFrequency(comp()->getOptions()->getProfilingFrequency());
         else
            profileInfo->setProfilingFrequency(profilingFreqTable  [numBackEdges]);
         if (comp()->getOptions()->getProfilingCount() != DEFAULT_PROFILING_COUNT)
            profileInfo->setProfilingCount(comp()->getOptions()->getProfilingCount());
         else
            profileInfo->setProfilingCount    (profilingCountsTable[numBackEdges]);
         }
      }

   // Remove structure - we are not going to maintain it
   //
   _cfg = comp()->getFlowGraph();
   _cfg->setStructure(NULL);

   // Create a second version of all the blocks in the method. At asynccheck
   // points there will be jumps between the original version and the second
   // version.
   // Most of the method execution will be in the original version of the
   // method. When a counting threshold is reached at an asynccheck, control
   // will jump to the second version of the method which contains all the
   // profiling code. At the next asynccheck point control will be returned to
   // the original version of the method.

   if (trace())
      {
      traceMsg(comp(), "Starting Profile Generation for %s\n", comp()->signature());
      comp()->dumpMethodTrees("Trees before Profile Generation");
      }

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   // Prepare the original blocks by moving async checks to the start of their
   // extended block and splitting the block at the async check
   //
   dumpOptDetails(comp(), "%s prepare blocks\n", OPT_DETAILS);
   prepareBlocks();

   // Make a copy of all the original blocks and join where appropriate
   //
   dumpOptDetails(comp(), "%s generate profiling body\n", OPT_DETAILS);
   createProfiledMethod();

   if (_asyncTree)
      {
      TR::TreeTop *prev = _asyncTree->getPrevTreeTop();
      TR::TreeTop *next = _asyncTree->getNextTreeTop();
      prev->join(next);
      _asyncTree->getNode()->recursivelyDecReferenceCount();
      }

   } // stack memory region scope

   if (trace())
      {
      comp()->dumpMethodTrees("Trees after Profile Generation");
      traceMsg(comp(), "Ending Profile Generation");
      }

   return 2;
   }

const char *
TR_ProfileGenerator::optDetailString() const throw()
   {
   return "O^O PROFILE GENERATOR: ";
   }

TR::Node *TR_ProfileGenerator::copyRegDeps(TR::Node *from, bool shareChildren)
   {
   TR_ASSERT(from->getOpCodeValue() == TR::GlRegDeps, "expecting TR::GlRegDeps node");
   TR::Node *node = TR::Node::copy(from);
   for (int32_t i = from->getNumChildren()-1; i >= 0; i--)
      {
      TR::Node *child = from->getChild(i);
      if (shareChildren)
         child->incReferenceCount();
      else
         {
         child = TR::Node::copy(child);
         child->setReferenceCount(1);
         node->setChild(i, child);
         }
      }
   return node;
   }

int32_t TR_ProfileGenerator::prepareBlocks()
   {
   // Move all asynccheck trees to the start of their extended basic block.
   // This makes sure that splitting the blocks at these points will not
   // cause a problem with multiply-referenced nodes.
   // Split the blocks at the asynccheck trees.
   //
   TR::Block   *block = NULL, *processedBlock = NULL;
   TR::TreeTop *treeTop, *nextTreeTop;
   TR::Node    *node;
   int32_t    numAsyncchecks = 0;

#if DEBUG
   splitPoint = 0;
#endif

   TR::Block *startBlock = NULL;
   for (treeTop = comp()->getStartTree(); treeTop; treeTop = nextTreeTop)
      {
      _lastOriginalTree = treeTop;
      nextTreeTop = treeTop->getNextTreeTop();
      node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::BBStart)
         {
         // Remember the start of the current extended block
         //
         if (!node->getBlock()->isExtensionOfPreviousBlock())
            {
            block = node->getBlock();

            startBlock = NULL;
            if (treeTop == comp()->getStartTree())
               startBlock = block;
            }
         }
      else if (node->getOpCodeValue() == TR::asynccheck)
         {
         numAsyncchecks++;

#if DEBUG
         splitPoint++;
         if (splitPoint < FIRST_SPLIT_POINT || splitPoint > LAST_SPLIT_POINT)
            continue;
#endif
         // If this extended block already has an asynccheck, leave this one
         // in place.
         //
         // We do not want to move the asynccheck to the start of the method if
         // escape analysis can create local objects which need to be initialized in the first block
         // The async check (if moved) would be a GC point causing the GC to see
         // an uninitialized local object
         //
         if ((block == processedBlock) || (block == startBlock))
            continue;

         dumpOptDetails(comp(), "%s    moving asyncCheck [" POINTER_PRINTF_FORMAT "] to start of block_%d\n",
            OPT_DETAILS, node, block->getNumber());

         // Remove the asynccheck from its current position
         //
         treeTop->getPrevTreeTop()->setNextTreeTop(nextTreeTop);
         nextTreeTop->setPrevTreeTop(treeTop->getPrevTreeTop());

         // Add the asynccheck to the start of the current extended block
         //
         block->prepend(treeTop);
         processedBlock = block;

         // Split the start of the extended basic block after the asynccheck
         // tree - this will be a point of crossover between the original
         // blocks and the second version containing the profiling code.
         // Make sure the regdeps on the BBStart and BBEnd nodes are correct.
         //
         block = processedBlock->split(treeTop->getNextTreeTop(), _cfg);
         if (processedBlock->getLiveLocals())
            block->setLiveLocals(new (trHeapMemory()) TR_BitVector(*processedBlock->getLiveLocals()));
         node = processedBlock->getEntry()->getNode();
         if (node->getNumChildren() > 0)
            {
            TR::Node *node2 = block->getEntry()->getNode();
            node2->setNumChildren(1);
            node2->setFirst(node->getFirstChild());
            node->setFirst(copyRegDeps(node2->getFirstChild(), false));
            node = node->getFirstChild();
            node2 = processedBlock->getExit()->getNode();
            node2->setNumChildren(1);
            node2->setFirst(copyRegDeps(node, true));
            }
         }
      }

   // Add a new block at the start of the method, with an asynccheck node so
   // that it becomes a split point
   //

   node = comp()->getStartTree()->getNode();
   block = TR::Block::createEmptyBlock(node, comp(), node->getBlock()->getFrequency());
   treeTop = TR::TreeTop::create(comp(), block->getEntry(), TR::Node::createWithSymRef(node, TR::asynccheck,0,comp()->getSymRefTab()->findOrCreateAsyncCheckSymbolRef(comp()->getMethodSymbol())));
   _asyncTree = treeTop;
   _cfg->insertBefore(block, node->getBlock());
   _cfg->addEdge(_cfg->getStart(), block);
   _cfg->removeEdge(_cfg->getStart(), node->getBlock());

   dumpOptDetails(comp(), "%s    adding block_%d to start of method\n", OPT_DETAILS, block->getNumber());

   if (node->getNumChildren() > 0)
      {
      TR::Node *node2 = block->getEntry()->getNode();
      node2->setNumChildren(1);
      TR::Node *regDeps = node2->setFirst(copyRegDeps(node->getFirstChild(), false));
      //node->setFirst(copyRegDeps(node2->getFirstChild(), false));
      //node = node->getFirstChild();
      node2 = block->getExit()->getNode();
      node2->setNumChildren(1);
      node2->setFirst(copyRegDeps(regDeps, true));
      }

   comp()->setStartTree(block->getEntry());
   _firstOriginalTree = comp()->getStartTree();

   return numAsyncchecks;
   }

void TR_ProfileGenerator::createProfiledMethod()
   {

   // Clone all the blocks in the method and add the cloned trees to the end
   // of the original list of trees
   //
   TR_BlockCloner cloner(_cfg, false, true);
   _lastOriginalTree->join(cloner.cloneBlocks(_firstOriginalTree->getNode()->getBlock(), _lastOriginalTree->getNode()->getBlock())->getEntry());

   // Go through all the original trees.
   //
   // Any profiling trees are removed from the original blocks
   //
   // Blocks containing an asynccheck have been prepared to look like this:
   //
   //      Original                              Clone
   //      --------                              -----
   //   O1:  BBStart                          C1:  BBStart
   //        asynccheck                            asynccheck
   //        BBEnd                                 BBEnd
   //   O1a: BBStart                          C1a: BBStart
   //        ... rest of block                     ... rest of block
   //        BBEnd                                 BBEnd
   //
   //
   //
   // This is changed to look like this:
   // HIGH LEVEL OVERVIEW
   //=====================
   // At the beginning of the method we add the following code
   // /* select the correct frequency and cntr version, based on recompilCounter */
   // int *freqPtr = &frequencyArray[0];
   // int *cntrPtr = &cntrArray[0];
   // int tempRC = recompilationCounter;
   // if (tempRC < PROFILING_INVOCATION_COUNT) { //*** see NOTE below
   //   freqPtr = freqPtr + (tempRC << 2);
   //   cntrPtr = cntrPtr + (tempRC << 2);
   // }
   //
   // The Original block will add the following code
   //
   // blockO1a:
   // int t = *freqPtr - 1;
   // *freqPtr = t;
   // if (t <= 0) goto blockC1b
   //
   // The cloned block will add the following code
   //
   // blockC1b:
   // int tempFreq = RESET_VALUE; (47)
   // int t1 = *cntrPtr - 1;
   // if (t1 > 0) goto blockC1a
   // blockC1c:
   //   tempFreq = INT_MAX;
   //   recompilationCounter--;
   // blockC1a:
   // *freqPtr = tempFreq;
   //
   // NOTE: frequencyArray is an array in TR_PersistentProfileInfo that
   // stores the frequency of profiling for each pass, i.e the number of
   // times we execute the main line code before going to the profiled code
   // The actual name is _profilingFrequency[PROFILING_INVOCATION_COUNT]
   // cntrArray is an in array in TR_PersistentProfileInfo that stores
   // the number of profiling samples (transitions from original to profiling code)
   // -per pass- that we perform before stopping profiling
   // The actual name is _profilingCount[PROFILING_INVOCATION_COUNT]
   // The number of profiling passes (and the size of these two arrays)
   // is given by PROFILING_INVOCATION_COUNT which currently is set to 2
   // The recompilationCounter - which denotes the current profiling pass -
   // starts from PROFILING_INVOCATION_COUNT, and is decremented until it reaches -1
   // At that point the method will be compiled at scorching level and recompilationCounter
   // will be set to 10000.
   // When accessing the frequencyArray and cntrArray we have to make sure
   // that the index (tempRC) is within the array bounds. This is accomplished by
   // the test: if (tempRC < PROFILING_INVOCATION_COUNT).  The test is performed
   // using unsigned integers so that, when tempRC becomes negative, the test fails.

   // Below is the description of the old implementation which had some races
   //
   //
   //      Original                              Clone
   //      --------                              -----
   //   O1:  BBStart                          C1:  BBStart
   //        asynccheck
   //        istore frequency                      asynccheck
   //           isub                               goto --> O1a
   //              iload frequency            C1b: BBStart
   //              iconst 1
   //        ificmple --> C1b                      istore frequency
   //           iload frequency                       iconst resetValue
   //           iconst 0                           istore temp
   //        BBEnd                                    isub
   //   O1a: BBStart                                     iload count
   //        ... rest of block                           iconst 1
   //        BBEnd                                 ificmpgt --> C1a
   //                                                 iload temp
   //                                                 iconst 0
   //                                              BBEnd
   //                                         C1c: BBStart
   //                                              istore frequency
   //                                                 iconst INT_MAX
   //                                              dec recompilationCounter
   //                                              BBEnd
   //                                         C1a: BBStart
   //                                              memory fence
   //                                              istore count
   //                                                 iload
   //                                              ... rest of block
   //                                              BBEnd
   //
   TR::TreeTop *treeTop = NULL, *nextTreeTop = NULL;
   TR::Node    *node = NULL, *newNode = NULL;
   TR::Block   *blockO1 = NULL, *blockO1a = NULL;
   TR::Block   *blockC1 = NULL, *blockC1a = NULL, *blockC1b = NULL, *blockC1c = NULL;
   TR::Block   *processedBlock = NULL;

   TR::Recompilation *recompilationInfo   = comp()->getRecompilationInfo();
   TR_PersistentProfileInfo *profileInfo = TR_PersistentProfileInfo::getCurrent(comp());

   TR::SymbolReference *recompilationCounterSymRef;
   recompilationCounterSymRef = recompilationInfo->getCounterSymRef();

   TR::SymbolReference *freqPtrSymRef = comp()->getSymRefTab()->createTemporary(
            comp()->getMethodSymbol(), TR::Address);
   freqPtrSymRef->getSymbol()->setNotCollected(); // prevent GC messing with it
   TR::SymbolReference *cntrPtrSymRef = comp()->getSymRefTab()->createTemporary(
            comp()->getMethodSymbol(), TR::Address);
   cntrPtrSymRef->getSymbol()->setNotCollected(); // prevent GC messing with it

   TR::SymbolReference *tempRcSymRef = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Int32);
   TR::SymbolReference *tempFreqSymRef = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Int32);
   TR::SymbolReference *tSymRef = comp()->getSymRefTab()->findOrCreateGenericIntShadowSymbolReference(0);
#if DEBUG
   splitPoint = -1;
#endif

   bool firstSplitPoint = true;
   for (treeTop = _firstOriginalTree; treeTop != _lastOriginalTree; treeTop = nextTreeTop)
      {
      nextTreeTop = treeTop->getNextTreeTop();
      node = treeTop->getNode();
      // This produces a lot of information-free lines
      //dumpOptDetails(comp(), "%s    processing node [" POINTER_PRINTF_FORMAT "]\n", OPT_DETAILS, node);
      if (node->getOpCodeValue() == TR::BBStart)
         {
         // Remember the start of the current extended block
         //
         if (!node->getBlock()->isExtensionOfPreviousBlock())
            blockO1 = node->getBlock();
         }
      else if (node->isProfilingCode())
         {
         // Remove profiling code from the original trees
         // But anchor the first child of a call node back into the original chain
         //
         if (node->getOpCode().isTreeTop() && node->getFirstChild()->getOpCode().isCall())
            {
            TR::Node *firstChild = node->getFirstChild()->getFirstChild();
            firstChild->incReferenceCount();
            node->getFirstChild()->recursivelyDecReferenceCount();
            node->setFirst(firstChild);
            }
         else
            TR::TransformUtil::removeTree(comp(), treeTop);
         }
      else if (node->getOpCodeValue() == TR::asynccheck)
         {
#if DEBUG
         splitPoint++;
         if (splitPoint < FIRST_SPLIT_POINT || splitPoint > LAST_SPLIT_POINT)
            continue;
#endif
         // If this extended block already has an asynccheck, leave this one
         // in place.
         //
         if (blockO1 == processedBlock)
            continue;

         // Change the asynccheck code as shown in the comments above
         //
         blockO1a = blockO1->getNextBlock();
         blockC1  = cloner.getToBlock(blockO1);
         blockC1a = blockC1->getNextBlock();
         blockC1b = TR::Block::createEmptyBlock(node, comp(), blockC1->getFrequency());
         blockC1c = TR::Block::createEmptyBlock(node, comp(), blockC1->getFrequency());
         processedBlock = blockO1a;

         // Find Regdeps information
         //
         newNode = blockO1->getEntry()->getNode();
         TR::Node *regDeps = (newNode->getNumChildren() > 0) ? newNode->getFirstChild() : NULL;

         if (firstSplitPoint)
            {
            // Some special case code related to decrementing invocation count
            // needs to be inserted only at the beginning of the method (i.e. at
            // the first split point
            //
            TR::Block *blockO1b = TR::Block::createEmptyBlock(node, comp(), blockO1->getFrequency());
            TR::Block *blockO1c = TR::Block::createEmptyBlock(node, comp(), blockO1->getFrequency());

            blockO1->getExit()->join(blockO1b->getEntry());
            blockO1b->getExit()->join(blockO1c->getEntry());
            blockO1c->getExit()->join(blockO1a->getEntry());

            // assign pointer     freqPtr = &frequencyArray[0]
            TR::Node *addrNode = TR::Node::aconst(node, (uintptrj_t)profileInfo->getProfilingFrequencyArray());

            newNode = TR::Node::createWithSymRef(TR::astore, 1, 1, addrNode, freqPtrSymRef);
            treeTop = TR::TreeTop::create(comp(), treeTop, newNode);

            // assign pointer     cntrPtr = &cntrArray[0]
            addrNode = TR::Node::aconst(node, (uintptrj_t)profileInfo->getProfilingCountArray());

            newNode = TR::Node::createWithSymRef(TR::astore, 1, 1, addrNode, cntrPtrSymRef);
            treeTop = TR::TreeTop::create(comp(), treeTop, newNode);

            // read recompilationCounter in a temporary variable tempRc
            TR::Node *rcNode;
            rcNode = TR::Node::createWithSymRef(node, TR::iload, 0, recompilationCounterSymRef);

            newNode = TR::Node::createWithSymRef(TR::istore, 1, 1, rcNode, tempRcSymRef);
            treeTop = TR::TreeTop::create(comp(), treeTop, newNode);

            newNode = TR::Node::createif(
               TR::ifiucmpge,
               rcNode,
               TR::Node::create(node, TR::iconst, 0, PROFILING_INVOCATION_COUNT),
               blockO1c->getEntry());

            _cfg->addNode(blockO1b);
            _cfg->addNode(blockO1c);

            _cfg->addEdge(blockO1, blockO1c);
            _cfg->addEdge(blockO1, blockO1b);
            _cfg->addEdge(blockO1b, blockO1c);
            _cfg->addEdge(blockO1c, blockO1a);
            _cfg->removeEdge(blockO1, blockO1a);

            if (regDeps)
               {
               newNode->setNumChildren(3);
               newNode->setChild(2, copyRegDeps(regDeps, true));
               }

            TR::TreeTop::create(comp(), treeTop, newNode);

            treeTop = blockO1b->getEntry();
            if (regDeps)
               {
               newNode = blockO1b->getEntry()->getNode();
               newNode->setNumChildren(1);
               regDeps = newNode->setFirst(copyRegDeps(regDeps, false));
               newNode = blockO1b->getExit()->getNode();
               newNode->setNumChildren(1);
               newNode->setFirst(copyRegDeps(regDeps, true));

               newNode = blockO1c->getEntry()->getNode();
               newNode->setNumChildren(1);
               regDeps = newNode->setFirst(copyRegDeps(regDeps, false));
               newNode = blockO1c->getExit()->getNode();
               newNode->setNumChildren(1);
               newNode->setFirst(copyRegDeps(regDeps, true));
               }

            if (blockO1->getLiveLocals())
               {
               blockO1b->setLiveLocals(new (trHeapMemory()) TR_BitVector(*blockO1->getLiveLocals()));
               blockO1c->setLiveLocals(new (trHeapMemory()) TR_BitVector(*blockO1->getLiveLocals()));
               }

            // make freqPtr = &freqArray[tempRC] and cntrPtr = &cntrArray[tempRC]
            TR::Node *offsetNode = TR::Node::create(TR::imul, 2,
                                        TR::Node::createWithSymRef(node, TR::iload, 0, tempRcSymRef),
                                        TR::Node::create(node, TR::iconst, 0, sizeof(int32_t)));
            // NOTE: above sizeof(int32_t) represents the size of one element in the
            // freqArray and cntrArray (defined in TR_PersistentProfileInfo);
            // If the type of these arrays changes the future, this code will need to be adjusted
            TR::ILOpCodes opCode = TR::aiadd;
            if (TR::Compiler->target.is64Bit())
               {
               offsetNode = TR::Node::create(TR::i2l, 1, offsetNode);
               opCode = TR::aladd;
               }

            TR::Node *sumNode = TR::Node::create(opCode, 2,
                                               TR::Node::createWithSymRef(node, TR::aload, 0, freqPtrSymRef),
                                               offsetNode);
            newNode = TR::Node::createWithSymRef(TR::astore, 1, 1, sumNode, freqPtrSymRef);
            treeTop = TR::TreeTop::create(comp(), treeTop, newNode);

            sumNode = TR::Node::create(opCode, 2,
                                      TR::Node::createWithSymRef(node, TR::aload, 0, cntrPtrSymRef),
                                      offsetNode);
            newNode = TR::Node::createWithSymRef(TR::astore, 1, 1, sumNode, cntrPtrSymRef);
            TR::TreeTop::create(comp(), treeTop, newNode);

            treeTop = blockO1c->getEntry();
            blockO1 = blockO1c;
            }

         // Set up block O1
         //

         // read in *freqPtr
         TR::Node *freqPtrNode = TR::Node::createWithSymRef(node, TR::aload, 0, freqPtrSymRef);
         TR::Node *freqNode = TR::Node::createWithSymRef(TR::iloadi, 1, 1,
                                             freqPtrNode,
                                             tSymRef);

         TR::Block *blockO1Cont = blockO1->splitEdge(blockO1, blockO1a, comp(), NULL, false);
         if (blockO1->getLiveLocals())
            blockO1Cont->setLiveLocals(new (trHeapMemory()) TR_BitVector(*blockO1->getLiveLocals()));

         treeTop = TR::TreeTop::create(comp(), treeTop, TR::Node::create(TR::treetop, 1, freqNode));
         newNode = TR::Node::createif(TR::ificmpeq,
                                       freqNode,
                                       TR::Node::create(node, TR::iconst, 0, INT_MAX),
                                       blockO1a->getEntry());
         TR::TreeTop::create(comp(), treeTop, newNode);
         if (regDeps)
            {
            newNode->setNumChildren(3);
            newNode->setChild(2, copyRegDeps(regDeps, true));
            }
         _cfg->addEdge(blockO1, blockO1a);


         treeTop = blockO1Cont->getEntry();

         if (blockO1->getExit()->getNextTreeTop() == blockO1Cont->getEntry())
            blockO1Cont->setIsExtensionOfPreviousBlock();

         if (regDeps)
            {
            blockO1Cont->getExit()->getNode()->setNumChildren(1);
            blockO1Cont->getExit()->getNode()->setChild(0, copyRegDeps(regDeps, true));
            }

         if (blockO1Cont->isExtensionOfPreviousBlock())
            {
            if (regDeps)
               blockO1->getExit()->getNode()->removeChild(0);
            }
         else
            {
            freqPtrNode = TR::Node::createWithSymRef(node, TR::aload, 0, freqPtrSymRef);
            freqNode = TR::Node::createWithSymRef(TR::iloadi, 1, 1,
                                             freqPtrNode,
                                             tSymRef);
            }

         // decrement
         TR::Node *decrNode = TR::Node::create(TR::iadd, 2,
                                             freqNode,
                                             TR::Node::create(node, TR::iconst, 0, -1));
         // copy data back to *freqPtr
         newNode = TR::Node::createWithSymRef(TR::istorei, 2, 2,
                                   freqPtrNode,
                                   decrNode,
                                   tSymRef);
         treeTop = TR::TreeTop::create(comp(), treeTop, newNode);

         // test value that we just decremented
         newNode = TR::Node::createif(TR::ificmple,
                                     decrNode,
                                     TR::Node::create(node, TR::iconst, 0, 0),
                                     blockC1b->getEntry());
         TR::TreeTop::create(comp(), treeTop, newNode);
         if (regDeps)
            {
            newNode->setNumChildren(3);
            newNode->setChild(2, copyRegDeps(regDeps, true));
            }
         _cfg->addEdge(blockO1Cont, blockC1b);

         // Set up block C1c
         //
         // tempFreq = INT_MAX;
         newNode = TR::Node::createWithSymRef(TR::istore, 1, 1,
                                   TR::Node::create(node, TR::iconst, 0, INT_MAX),
                                   tempFreqSymRef);
         treeTop = TR::TreeTop::create(comp(), blockC1c->getEntry(), newNode);

         // recompilationCounter--;
         TR::StaticSymbol * symbol = recompilationCounterSymRef->getSymbol()->castToStaticSymbol();
         TR::DataType type = symbol->getDataType();
         if (comp()->cg()->getAccessStaticsIndirectly() && !recompilationCounterSymRef->isUnresolved() && type != TR::Address)
            {
            TR::SymbolReference * symRefShadow = comp()->getSymRefTab()->createKnownStaticDataSymbolRef(symbol->getStaticAddress(), type);
            TR::Node * loadaddrNode = TR::Node::createWithSymRef(node, TR::loadaddr, 0, symRefShadow);
            newNode = TR::Node::createWithSymRef(TR::istorei, 2, 2, loadaddrNode,
                                      TR::Node::create(TR::iadd, 2,
                                      TR::Node::createWithSymRef(TR::iloadi, 1, 1, loadaddrNode, recompilationCounterSymRef),
                                      TR::Node::create(node, TR::iconst, 0, -1)),
                                      recompilationCounterSymRef);
            }
         else
            {
            newNode = TR::Node::createWithSymRef(TR::istore, 1, 1,
               TR::Node::create(
                  TR::iadd,
                  2,
                  TR::Node::createWithSymRef(node, TR::iload, 0, recompilationCounterSymRef),
                  TR::Node::create(node, TR::iconst, 0, -1)),
               recompilationCounterSymRef);
            }
         treeTop = TR::TreeTop::create(comp(), treeTop, newNode);

         if (regDeps)
            {
            newNode = blockC1c->getEntry()->getNode();
            newNode->setNumChildren(1);
            regDeps = newNode->setFirst(copyRegDeps(regDeps, false));
            newNode = blockC1c->getExit()->getNode();
            newNode->setNumChildren(1);
            newNode->setFirst(copyRegDeps(regDeps, true));
            }
         if (blockO1->getLiveLocals())
            blockC1c->setLiveLocals(new (trHeapMemory()) TR_BitVector(*blockO1->getLiveLocals()));
         _cfg->insertBefore(blockC1c, blockC1a);

         // Set up block C1b
         //
         if (regDeps)
            {
            newNode = blockC1b->getEntry()->getNode();
            newNode->setNumChildren(1);
            regDeps = newNode->setFirst(copyRegDeps(regDeps, false));
            }

         // tempFreq = VALUE (47)
         newNode = TR::Node::createWithSymRef(TR::istore, 1, 1,
                                   TR::Node::create(node, TR::iconst, 0, recompilationInfo->getProfilingFrequency()),
                                   tempFreqSymRef);
         treeTop = TR::TreeTop::create(comp(), blockC1b->getEntry(), newNode);

         // read in the profiling counter (*cntrPtr)
         TR::Node *cntrPtrNode = TR::Node::createWithSymRef(node, TR::aload, 0, cntrPtrSymRef);
         TR::Node *cntrNode = TR::Node::createWithSymRef(TR::iloadi, 1, 1,
                                             cntrPtrNode,
                                             tSymRef);
         // decrement it
         decrNode = TR::Node::create(TR::iadd, 2,
                                    cntrNode,
                                    TR::Node::create(node, TR::iconst, 0, -1));
         // copy data back to *cntrPtr
         newNode = TR::Node::createWithSymRef(TR::istorei, 2, 2,
                                   cntrPtrNode,
                                   decrNode,
                                   tSymRef);

         treeTop = TR::TreeTop::create(comp(), treeTop, newNode);

         newNode = TR::Node::createif(TR::ificmpgt,
                                     decrNode,
                                     TR::Node::create(node, TR::iconst, 0, 0),
                                     blockC1a->getEntry());
         treeTop = TR::TreeTop::create(comp(), treeTop, newNode);
         if (regDeps)
            {
            newNode->setNumChildren(3);
            newNode->setChild(2, copyRegDeps(regDeps, true));
            newNode = blockC1b->getExit()->getNode();
            newNode->setNumChildren(1);
            newNode->setFirst(copyRegDeps(regDeps, true));
            }
         if (blockO1->getLiveLocals())
            blockC1b->setLiveLocals(new (trHeapMemory()) TR_BitVector(*blockO1->getLiveLocals()));
         _cfg->insertBefore(blockC1b, blockC1c);
         blockC1->getExit()->join(blockC1b->getEntry());

         // add some residual code to C1a

         // *freqPtr = tempFreq;
         newNode = TR::Node::createWithSymRef(TR::istorei, 2, 2,
                                   TR::Node::createWithSymRef(node, TR::aload, 0, freqPtrSymRef),
                                   TR::Node::createWithSymRef(node, TR::iload, 0, tempFreqSymRef),
                                   tSymRef);
         TR::TreeTop::create(comp(), blockC1a->getEntry(), newNode);


         // Set up block C1
         //
         if(recompilationInfo->getProfilingFrequency()==1)
            {
            // read in *freqPtr
            TR::Node *freqPtrNode = TR::Node::createWithSymRef(node, TR::aload, 0, freqPtrSymRef);
            TR::Node *freqNode = TR::Node::createWithSymRef(TR::iloadi, 1, 1,
                                                freqPtrNode,
                                                tSymRef);
           newNode = TR::Node::createif(TR::ificmpgt,
                                       freqNode,
                                       TR::Node::create(node, TR::iconst, 0, recompilationInfo->getProfilingFrequency()),
                                       blockO1a->getEntry());
           TR::TreeTop::create(comp(), blockC1->getExit()->getPrevTreeTop(), newNode);
           if(regDeps)
              {
              regDeps = blockC1->getExit()->getNode()->getFirstChild();
              newNode->setNumChildren(3);
              newNode->setChild(2, copyRegDeps(regDeps, true));
              }
           _cfg->addEdge(blockC1, blockC1b);
           }
        else
           {
              newNode = TR::Node::create(node, TR::Goto, 0, blockO1a->getEntry());
              TR::TreeTop::create(comp(), blockC1->getExit()->getPrevTreeTop(), newNode);
              if(regDeps)
                 {
                 regDeps = blockC1->getExit()->getNode()->getFirstChild();
                 newNode->setNumChildren(1);
                 newNode->setFirst(regDeps);
                 blockC1->getExit()->getNode()->setFirst(NULL);
                 blockC1->getExit()->getNode()->setNumChildren(0);
                 }
           }
         _cfg->addEdge(blockC1, blockO1a);
         _cfg->removeEdge(blockC1, blockC1a);
         firstSplitPoint = false;
         }
      }
   }










