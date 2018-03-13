/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
#include "JProfilingRecompLoopTest.hpp"
#include "il/Block.hpp"
#include "infra/Cfg.hpp"
#include "infra/TRCfgEdge.hpp"
#include "infra/Checklist.hpp"
#include "infra/ILWalk.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "il/Node_inlines.hpp"
#include "infra/Checklist.hpp"             // for TR::NodeChecklist
#include "ras/DebugCounter.hpp"
#include "control/Recompilation.hpp"              // for TR_Recompilation, etc
#include "control/RecompilationInfo.hpp"              // for TR_Recompilation, etc
#include "codegen/CodeGenerator.hpp"
#include "optimizer/TransformUtil.hpp"            // for TransformUtil
#include "optimizer/JProfilingBlock.hpp"


/**
 * Walks the TR_RegionStructure counting loops to get the nesting depth of the block
 */ 
int32_t TR_JProfilingRecompLoopTest::getLoopNestingDepth(TR::Compilation *comp, TR::Block *block)
   {
   TR_RegionStructure *region = block->getParentStructureIfExists(comp->getFlowGraph());
   int32_t nestingDepth = 0;
   while (region && !region->isAcyclic())
      {
      nestingDepth++;
      region = region->getParent();
      }
   return nestingDepth;
   }

/**
 * A utility function that iterates through the TR::list of byte code info and checks if passed bytecode info exists in list
 * by checking bytecode index and caller index of the bytecode info
 */ 
bool
TR_JProfilingRecompLoopTest::isByteCodeInfoInCurrentTestLocationList(TR_ByteCodeInfo &bci, TR::list<TR_ByteCodeInfo, TR::Region&> &addedLocationBCIList)
   {
   for (auto iter = addedLocationBCIList.begin(), listEnd = addedLocationBCIList.end(); iter != listEnd; ++iter)
      {
      TR_ByteCodeInfo iterBCI = *iter;
      if (iterBCI.getByteCodeIndex() == bci.getByteCodeIndex() && iterBCI.getCallerIndex() == bci.getCallerIndex())
         return true;   
      }
   return false;
   }

int32_t
TR_JProfilingRecompLoopTest::perform()
   {
   if (comp()->getOption(TR_EnableJProfiling))
      {
      if (trace())
         traceMsg(comp(), "JProfiling has been enabled, run JProfiling\n");
      }
   else if (comp()->getProfilingMode() == JProfiling)
      {
      if (trace())
         traceMsg(comp(), "JProfiling has been enabled for profiling compilations, run JProfilingRecompLoopTest\n");
      }
   else
      {
      if (trace())
         traceMsg(comp(), "JProfiling has not been enabled, skip JProfilingRecompLoopTest\n");
      return 0;
      }
   RecompilationTestLocations testLocations(RecompilationTestLocationAllocator(comp()->trMemory()->currentStackRegion()));
   TR::TreeTop *cursor = comp()->getStartTree();
   TR::CFG *cfg = comp()->getFlowGraph();
   TR::Block *currentBlock = NULL;
   TR::list<TR_ByteCodeInfo, TR::Region&> addedLocationBCIList(comp()->trMemory()->currentStackRegion());
   while (cursor)
      {
      if (cursor->getNode()->getOpCodeValue() == TR::BBStart)
         {
         currentBlock = cursor->getNode()->getBlock();
         /**
           * As we are already walking down the tree tops, we can get the enclosing block by tracking TR::BBStart nodes and currentBlock contains
           * this information. 
           * We also keep local list of ByteCodeInfo for each test locations (asyncchecknode) in the extended basic blocks.
           * This can avoid adding multiple tests for multiple locations with same byte code info in extended basic block.
           * As soon as we encounter a block which is not extenstion of previous block, we clear the list of byte code info.
           */    
         if (!currentBlock->isExtensionOfPreviousBlock() && !addedLocationBCIList.empty())
            addedLocationBCIList.clear();
         }
      else if (cursor->getNode()->getOpCodeValue() == TR::asynccheck)
         {
         TR_ASSERT_FATAL(currentBlock != NULL,"We should have encountered BBStart before and should have the enclosing block");
         if (currentBlock->getStructureOf()->getContainingLoop() != NULL)
            {
            TR_ByteCodeInfo bci = cursor->getNode()->getByteCodeInfo();
            // If the list of TR_ByteCodeInfo used to track the tree tops in extended basic blocks is empty or the bytecode info
            // of current asynccheck node does not match with any tracked bytecode info, we will add the test otherwise skip it.   
            if (addedLocationBCIList.empty() || !isByteCodeInfoInCurrentTestLocationList(bci, addedLocationBCIList))
               {
               addedLocationBCIList.push_back(bci);
               int32_t loopDepth = getLoopNestingDepth(comp(), currentBlock);
               testLocations.push_back(std::make_pair(std::make_pair(cursor, currentBlock),loopDepth));
               }
            }
         }
      cursor = cursor->getNextTreeTop(); 
      }
   if (!testLocations.empty())
      addRecompilationTests(comp(), testLocations);
   return 1;
   }

/**   \brief   Adds trees and control flow to check the loop raw frequency and trip recompilation of the method
 *             if it has spent enough time in the loop. 
 *    \details Iterates a list of recompilation test locations passed by callee and adds the following trees and blocks.
 *             -----------------
 *             |...            |    <-- originalBlock
 *             |testLocationTT |
 *             |...            |
 *             -----------------
 *             Test location block shown above becomes,
 *             ----------------------------------------------
 *             |...                                         |  <--- originalBlock
 *             |testLocationTT                              |
 *             |ifcmple goto remainingCodeBlock             |---------------------------------------
 *             |  rawFrequencyOfLoopContainingTestLocation  |                                      |
 *             |  recompilationThreshold                    |                                      |
 *             ----------------------------------------------                                      |
 *                                  |                                                              |
 *                                  |                                                              |
 *                                  V                                                              |
 *             -----------------------------------------------                                     |
 *             |call TR_jitRetranslateCallerWithPrep         | <--- callRecompilation block        |
 *             |       loadaddr startPC                      |                                     |
 *             |       loadaddr                              |                                     |
 *             -----------------------------------------------                                     |
 *                                  |                                                              |
 *                                  |                                                              |
 *                                  V                                                              |
 *                   --------------------------------                                              |
 *                   |treeTops after insertion Point|  <--- remainingCodeBlock                     |
 *                   |from original block           |                                              |
 *                   |...                           |<----------------------------------------------   
 *                   --------------------------------
 *    \param comp Current compilation object
 *    \param testLocations RecompilationTestLocation list containing TreeTops after which test are required to be added
 *                         And corresponding to that location, a loop nesting depth 
 */

void 
TR_JProfilingRecompLoopTest::addRecompilationTests(TR::Compilation *comp, RecompilationTestLocations &testLocations)
   {
   TR_PersistentProfileInfo *profileInfo = comp->getRecompilationInfo()->findOrCreateProfileInfo();
   TR_BlockFrequencyInfo *bfi = TR_BlockFrequencyInfo::get(profileInfo);
   TR::CFG *cfg = comp->getFlowGraph();
   cfg->setStructure(NULL);

   // Following environment sets up the base recompilation threshold for the loop.
   // This base recompile threshold in conjunction with the depth in loop is compared with the raw count of the
   // loop to decide if we have run this loop enough time to trip method recompilation. 
   // TODO: Currently set base recompilation threshold needs some tuning. 
   static char *p = feGetEnv("TR_LoopRecompilationThresholdBase");
   static int32_t recompileThreshold = p ? atoi(p) : 4096;
   // Iterating backwards to avoid losing original block associated with the test location tree tops in case we have found multiple
   // recompilation test location in same block.
   for (auto testLocationIter = testLocations.rbegin(), testLocationEnd = testLocations.rend(); testLocationIter != testLocationEnd; ++testLocationIter)
      {
      TR::TreeTop *asyncCheckTreeTop = testLocationIter->first.first;
      TR::Block *originalBlock = testLocationIter->first.second;
      TR::Node *node = asyncCheckTreeTop->getNode();
      int32_t depth = testLocationIter->second;
      if (trace())
         traceMsg(comp, "block_%d, n%dn, depth = %d\n",originalBlock->getNumber(), asyncCheckTreeTop->getNode()->getGlobalIndex(), depth);
      TR_ByteCodeInfo bci = asyncCheckTreeTop->getNode()->getByteCodeInfo();
      
      TR::Node *root = TR_JProfilingBlock::generateBlockRawCountCalculationNode(comp, bfi->getOriginalBlockNumberToGetRawCount(bci,comp), node, bfi);
      
      // If we got a bad counters/ bad block frequency info, above API would return NULL, if that is the case we can not generate a recompilation test for that location.
      if (!root)
         {
         TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "jprofiling.instrument/badcounters/(%s)", comp->signature()));
         continue;
         }
      dumpOptDetails(comp, "%s Add recompilation test after asyncCheck node n%dn\n", optDetailString(), node->getGlobalIndex());

      //Splitting the original block from AsyncCheckTT
      TR::Block *remainingCodeBlock = originalBlock->split(asyncCheckTreeTop->getNextTreeTop(), cfg, true, true);
      TR::Block *callRecompileBlock = TR::Block::createEmptyBlock(node, comp, UNKNOWN_COLD_BLOCK_COUNT);
      callRecompileBlock->setIsCold(true);
      
      TR::Node *callNode = TR::Node::createWithSymRef(node, TR::icall, 2, comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_jitRetranslateCallerWithPrep, false, false, true));
      callNode->setAndIncChild(0, TR::Node::createWithSymRef(node, TR::loadaddr, 0, comp->getSymRefTab()->findOrCreateStartPCSymbolRef()));
      callNode->setAndIncChild(1, TR::Node::createWithSymRef(node, TR::loadaddr, 0, comp->getSymRefTab()->findOrCreateCompiledMethodSymbolRef()));
      TR::TreeTop *callTree = TR::TreeTop::create(comp, TR::Node::create(node, TR::treetop, 1, callNode));
      callTree->getNode()->setIsProfilingCode();
      callRecompileBlock->append(callTree);
      cfg->addNode(callRecompileBlock);
      
      TR::Node *loadBaseThreshold = TR::Node::createWithSymRef(node, TR::iload, 0, comp->getSymRefTab()->createKnownStaticDataSymbolRef(&recompileThreshold, TR::Int32));
      //TR::Node *adjustThreshold = TR::Node::create(node, TR::ishl, 2, loadBaseThreshold, TR::Node::iconst(node, (depth-1)*3));
      // threshold for this particular test location is calculated from the base recompile threshold and the nesting depth of the loop
      // If threshold becomes larger than the maximum signed 32-Bit int then it will use the max value allowed as threshold.
      int32_t threshold = recompileThreshold << ((depth-1)*1);  
      TR::Node *cmpNode = TR::Node::createif(TR::ificmple, root, TR::Node::iconst(node, threshold > 0 ? threshold : TR::getMaxSigned<TR::Int32>()-1), remainingCodeBlock->getEntry());
      
      TR::TreeTop *cmpFlag = TR::TreeTop::create(comp, asyncCheckTreeTop, cmpNode);
      remainingCodeBlock->getEntry()->insertTreeTopsBeforeMe(callRecompileBlock->getEntry(), callRecompileBlock->getExit());
      cfg->addEdge(TR::CFGEdge::createEdge(callRecompileBlock, remainingCodeBlock, comp->trMemory()));
      cfg->addEdge(TR::CFGEdge::createEdge(originalBlock, callRecompileBlock, comp->trMemory()));
      if (trace())
         traceMsg(comp,"\t\t Newly created recompilation Test : n%dn in block_%d\n\t\tRecompilation Call in block_%d\n",
            callNode->getGlobalIndex(), originalBlock->getNumber(), callRecompileBlock->getNumber());
      }
   }

const char *
TR_JProfilingRecompLoopTest::optDetailString() const throw()
   {
   return "O^O JPROFILER RECOMP TEST: ";
   }