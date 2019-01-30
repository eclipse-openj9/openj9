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

#include "optimizer/SwitchAnalyzer.hpp"

#include <stdint.h>
#include <string.h>
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "env/IO.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "infra/TRCfgEdge.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/TransformUtil.hpp"

#define MIN_SIZE_FOR_BIN_SEARCH 4
#define MIN_CASES_FOR_OPT 4
#define SWITCH_TO_IFS_THRESHOLD 3
#define LOOKUP_SWITCH_GEN_IN_IL_OVERRIDE 15

#define ADD_BRANCH_TABLE_ADDRESS

#define OPT_DETAILS "O^O SWITCH ANALYZER: "

TR::SwitchAnalyzer::SwitchAnalyzer(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {
   // default values //FIXME
   _costMem    = 1;

   _costUnique = 3 + 6; // cost_cl + cost_bc
   _costRange  = 3 + 3 + 6; // cost_s + cost_cl + cost_bc
   _costDense  = _costRange + 10; // _costRange + cost_lba

   _minDensity = .01f;
   _binarySearchBound = 4;

   _smallDense = 1 + ((_costDense + _costMem)/_costUnique);
   }

int32_t TR::SwitchAnalyzer::perform()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   _cfg               = comp()->getFlowGraph();
   _haveProfilingInfo = (comp()->isOptServer()) ? _cfg->setFrequencies() : false;
   _blocksGeneratedByMe = new (trStackMemory()) TR_BitVector(_cfg->getNextNodeNumber(),
                                                       trMemory(), stackAlloc, growable);

   if (trace())
      {
      comp()->dumpMethodTrees("Trees Before Performing Switch Analysis");
      }

   TR::TreeTop *tt, *exitTree;
   for (tt = comp()->getStartTree();
        tt;
        tt = exitTree->getNextRealTreeTop())
      {
      TR::Block *block = tt->getNode()->getBlock();
      exitTree = block->getExit();

      TR::TreeTop *lastTree = exitTree->getPrevRealTreeTop();

      if (lastTree->getNode()->getOpCode().isSwitch())
         {
         switch (lastTree->getNode()->getOpCodeValue())
            {
            case TR::table:
            case TR::lookup:
               analyze(lastTree->getNode(), block);
               break;
            default:
               break;
            }
         }
      }

   if (trace())
      {
      comp()->dumpMethodTrees("Trees After Performing Switch Analysis");
      }

   return 1;
   }

const char *
TR::SwitchAnalyzer::optDetailString() const throw()
   {
   return "O^O SWITCH ANALYZER: ";
   }

void TR::SwitchAnalyzer::analyze(TR::Node *node, TR::Block *block)
   {
   if (_blocksGeneratedByMe->isSet(block->getNumber())) return;
   if (node->getFirstChild()->getOpCodeValue() == TR::trt) return;

   _switch     = node;
   _switchTree = block->getLastRealTreeTop();
   _defaultDest= node->getChild(1)->getBranchDestination();
   _block      = block;
   _nextBlock  = block->getNextBlock();
   _temp       = 0;

   if (_switch->getChild(0)->getType().isInt64())
      _isInt64 = true;
   else
      _isInt64 = false;

   int32_t *frequencies = setupFrequencies(node);
   CASECONST_TYPE lowVal = 0;
   CASECONST_TYPE highVal = 0;
   uint16_t upperBound = node->getCaseIndexUpperBound();

   if (upperBound <= 2)
      return;

   // Build Switch Info
   //
   TR_LinkHead<SwitchInfo> *chain = new (trStackMemory()) TR_LinkHead<SwitchInfo>();
   TR_LinkHead<SwitchInfo> *earlyUniques = new (trStackMemory()) TR_LinkHead<SwitchInfo>();
   for (int32_t i = upperBound - 1; i > 1; --i)
      {
      TR::Node *caseNode = node->getChild(i);
      CASECONST_TYPE konst;
      if (node->getOpCodeValue() == TR::table)
         konst = i - 2;
      else
         konst = caseNode->getCaseConstant();

      if (i == upperBound - 1)
         highVal = konst;
      if (i == 2)
         lowVal = konst;

      TR::TreeTop *target = caseNode->getBranchDestination();

      SwitchInfo *info = new (trStackMemory()) SwitchInfo(konst, target, _costUnique);
      if (frequencies)
         {
         info->_freq = ((float)frequencies[i]) / block->getFrequency();
         }

      if (trace())
         traceMsg(comp(), "Switch info pointing at target tree top 0x%p has frequency scale of %f\n", target->getNode(), info->_freq);

      if ((upperBound - 2) >= MIN_CASES_FOR_OPT && keepAsUnique(info, i))
         {
         info->setNext(earlyUniques->getFirst());
         earlyUniques->setFirst(info);
         }
      else
         chainInsert(chain, info);
      }

   // We are guaranteed that the case children are sorted.
   // Image the Z32 number lines
   //                [------------------------|--------------------------]                                                     signed
   //   TR::getMinSigned<TR::Int32>()         0            TR::getMaxSigned<TR::Int32>()      TR::getMaxUnsigned<TR::Int32>()
   //                                         [--------------------------|------------------------------------]                unsigned
   // If all the cases of the lookup are on the same 'half' of the number lines, it doesn't matter
   // if we use signed or unsigned comparisons, lets use signed.
   // For the signed case, we better use signed if the numbers straddle the 0 boundary; we already do.
   // For the unsigned case, we better use unsigned if the numbers straddle the TR::getMaxSigned<TR::Int32>() boundary. If that's
   // the case, the highVal will be less than (in the signed sense) the lowVal.
   //
   if (highVal < lowVal)
      _signed = false;
   else
      _signed = true;

   if (trace())
      {
      printInfo(comp()->fe(), comp()->getOutFile(), chain);
      traceMsg(comp(), "Early Unique Chain:\n");
      printInfo(comp()->fe(), comp()->getOutFile(), earlyUniques);
      }

   // Find Dense Sets
   //
   findDenseSets(chain);

   // Merge Dense Sets to form Larger Dense Sets
   //
   bool change=true;
   while (change)
      {
      change=mergeDenseSets(chain);
      }

   // Gather lonely uniq's and small dense nodes out of the main switch
   //
   TR_LinkHead<SwitchInfo> *bound = gather(chain);
   if (trace())
      {
      traceMsg(comp(), "Early Unique Chain:\n");
      printInfo(comp()->fe(), comp()->getOutFile(), earlyUniques);
      }

   // Remerge bound nodes back into the primary chain if small // FIXME: implement
   //
   //remerge(chain, bound);


   // Fix-up sorting order. If unsigned, we need to put it back to unsigned sorting order for correct emit since
   // merge part made it signed sorting order. The merge logic can be reused with this fix-up.
   // Caveats are that zero straddling set must be subdivided and TR::getMaxSigned<TR::Int32>() straddling set could be merged back.
   if (!_signed)
      {
      fixUpUnsigned(chain);
      fixUpUnsigned(bound);
      fixUpUnsigned(earlyUniques);
      if (trace())
         {
         traceMsg(comp(), "After fixing unsigned sort order\n");
         printInfo(comp()->fe(), comp()->getOutFile(), chain);
         printInfo(comp()->fe(), comp()->getOutFile(), bound);
         printInfo(comp()->fe(), comp()->getOutFile(), earlyUniques);
         }
      }

   // Emit
   //
   emit(chain, bound, earlyUniques);

   if (trace())
      traceMsg(comp(), "Done.\n");
   }

void TR::SwitchAnalyzer::findDenseSets(TR_LinkHead<SwitchInfo> *chain)
   {
   // find consecutive uniques (they must have different targets)
   // and group them together into a dense node
   //
   for (SwitchInfo *prev = 0, *cur = chain->getFirst();
        cur;
        prev = cur, cur = cur->getNext())
      {
      if (cur->_kind == Unique)
         {
         SwitchInfo *end = getConsecutiveUniques(cur);
         if (end != cur)
            {
            SwitchInfo *dense = new (trStackMemory()) SwitchInfo(trMemory());
            SwitchInfo *tail = end->getNext();

            for (SwitchInfo *t   = cur, *next = t->getNext();
                 t != tail;)
               {
               denseInsert(dense, t);
               t = next;
               if (!next)
                  break;
               next = next->getNext();
               }

            if (prev)
               prev->setNext(dense);
            else
               chain->setFirst(dense);

            dense->setNext(tail);
            cur = dense;
            }
         }
      }

   if (trace())
      {
      traceMsg(comp(), "After finding dense sets\n");
      printInfo(comp()->fe(), comp()->getOutFile(), chain);
      }

   }

bool TR::SwitchAnalyzer::mergeDenseSets(TR_LinkHead<SwitchInfo> *chain)
   {
   SwitchInfo *prev = 0;
   SwitchInfo *cur  = chain->getFirst();
   SwitchInfo *next = cur->getNext();
   bool change=false;

   while (next)
      {
      int32_t count = cur->_count + next->_count;
      CASECONST_TYPE span  = 1 + next->_max  - cur->_min;

      int32_t origCost = cur->_cost + next->_cost + _costRange;
      int32_t newCost  = _costDense + _costMem * span;

      if (newCost < origCost &&
          ((float)count / span) > _minDensity)
         {
         dumpOptDetails(comp(), "%smerging dense set %p\n", optDetailString(), cur);
         change=true;
         if (cur->_kind != Dense)
            {
            SwitchInfo *dense = new (trStackMemory()) SwitchInfo(trMemory());
            denseInsert(dense, cur);
            if (prev)
               prev->setNext(dense);
            else
               chain->setFirst(dense);
            cur = dense;
            }

         SwitchInfo *nextOfNext = next->getNext();
         denseInsert(cur, next);
         cur->setNext(nextOfNext);

         // go back one to process cur with the new next again
         //
         next = cur;
         cur  = prev;
         }

      prev = cur;
      cur  = next;
      next = next->getNext();
      }

   if (trace())
      {
      traceMsg(comp(), "After merging dense sets\n");
      printInfo(comp()->fe(), comp()->getOutFile(), chain);
      }
   return change;
   }

TR_LinkHead<TR::SwitchAnalyzer::SwitchInfo> *TR::SwitchAnalyzer::gather(TR_LinkHead<SwitchInfo> *chain)
   {
   SwitchInfo *prev = 0;
   SwitchInfo *cur  = chain->getFirst(); // will be non-null, chain cannot be empty
   SwitchInfo *next = cur->getNext();

   TR_LinkHead<SwitchInfo> *bound = new (trStackMemory()) TR_LinkHead<SwitchInfo>();

   // Remove lonely uniques and small denses from the chain
   //
   for (; cur; cur = next)
      {
      next = cur->getNext();
      dumpOptDetails(comp(), "%sgathering set %p\n", optDetailString(), cur);

      // ignore range nodes and dense nodes that are large
      //
      if ((cur->_kind == Range) ||
          (cur->_kind == Dense  &&  cur->_count >= _smallDense))
         {
         prev = cur;
         continue;
         }

      // Remove cur from the chain and insert it into the bound chain
      //
      if (prev)
         prev->setNext(next);
      else
         chain->setFirst(next);

      if (cur->_kind == Unique)
         chainInsert(bound, cur);
      else
         {
         // Split the Dense node
         //
         SwitchInfo *ptr = cur->_chain->getFirst();
         while (ptr)
            {
            SwitchInfo *next = ptr->getNext();
            chainInsert(bound, ptr);
            ptr = next;
            }
         }
      }

   if (trace())
      {
      traceMsg(comp(), "After Gathering\nPrimary Chain:\n");
      printInfo(comp()->fe(), comp()->getOutFile(), chain);
      traceMsg(comp(), "Bound Chain:\n");
      printInfo(comp()->fe(), comp()->getOutFile(), bound);
      }

   return bound;
   }


TR::SwitchAnalyzer::SwitchInfo *TR::SwitchAnalyzer::getConsecutiveUniques(SwitchInfo *info)
   {
   SwitchInfo *cur  = info;
   SwitchInfo *next = cur->getNext();

   while (next &&
          next->_kind == Unique &&
          next->_min == cur->_max+1)
      {
      cur = next;
      next = next->getNext();
      }
   return cur;
   }

bool TR::SwitchAnalyzer::SwitchInfo::operator >(SwitchInfo &other)
   {
   return (_min > other._max);
   }

// Inserts into a sorted linked list
//
void TR::SwitchAnalyzer::chainInsert(TR_LinkHead<SwitchInfo> *chain, SwitchInfo *info)
   {
   TR_ASSERT(info->_kind != Dense, "Not expecting a dense node");
   SwitchInfo *cur, *prev;
   for (prev = 0, cur = chain->getFirst();
        cur != NULL;
        prev = cur, cur = cur->getNext())
      if (*cur > *info)
         break;

   if (cur &&
       cur->_target == info->_target &&
       cur->_min    == info->_max+1)
      {
      if (cur->_kind != Range)
         {
         cur->_kind = Range;
         cur->_cost = _costRange;
         }
      cur->_min   = info->_min;
      cur->_freq += info->_freq;
      cur->_count+= info->_count;
      }
   else
      {
      info->setNext(cur);
      if (!prev)
         chain->setFirst(info);
      else
         prev->setNext(info);
      }
   }

void TR::SwitchAnalyzer::denseInsert(SwitchInfo *dense, SwitchInfo *info)
   {
   TR_ASSERT(dense->_kind == Dense, "assertion failure");

   if (info->_kind == Dense)
      {
      denseMerge(dense, info);
      return;
      }

   if (info->_kind == Range)
      {
      // Split the range into unique nodes, and insert the nodes
      for (CASECONST_TYPE i = info->_min; i <= info->_max; ++i)
         denseInsert(dense, new (trStackMemory()) SwitchInfo(i, info->_target, _costUnique));

      return;
      }

   // Info is a Unique Node
   //
   chainInsert(dense->_chain, info);

   if (info->_min < dense->_min)
      dense->_min = info->_min;
   if (info->_max > dense->_max)
      dense->_max = info->_max;

   dense->_freq += info->_freq;
   dense->_count+= info->_count;

   dense->_cost = _costDense + _costMem * dense->_count;
   }

void TR::SwitchAnalyzer::denseMerge(SwitchInfo *to, SwitchInfo *from)
   {
   SwitchInfo *cur  = from->_chain->getFirst();
   TR_ASSERT(cur, "a dense node must have >= 2 elements in the chain");

   while (cur)
      {
      SwitchInfo *next = cur->getNext();
      denseInsert(to, cur);
      cur = next;
      }
   }


void TR::SwitchAnalyzer::printInfo(TR_FrontEnd *fe, TR::FILE *pOutFile, TR_LinkHead<SwitchInfo> *chain)
   {
   if (!pOutFile) return;

   trfprintf(pOutFile, "------------------------------------------------ for lookup node [%p] in block_%d\n", _switch, _block->getNumber());

   for (SwitchInfo *info = chain->getFirst(); info; info = info->getNext())
      {
      info->print(fe, pOutFile, 0);
      }
   trfprintf(pOutFile, "================================================\n");
   trfflush(pOutFile);
   }

void TR::SwitchAnalyzer::SwitchInfo::print(TR_FrontEnd *fe, TR::FILE *pOutFile, int32_t indent)
   {
   if (!pOutFile) return;

   trfprintf(pOutFile, "%*s %0.8g %4d %8d [%4d -%4d] ",
             indent, " ", _freq, _count, _cost, _min, _max);
   switch (_kind)
      {
      case Unique:
         trfprintf(pOutFile, " -> %3d Unique\n", _target->getNode()->getBlock()->getNumber());
         break;
      case Range:
         trfprintf(pOutFile, " -> %3d Range\n", _target->getNode()->getBlock()->getNumber());
         break;
      case Dense:
         trfprintf(pOutFile, " [====] Dense\n");
         for (SwitchInfo *info = _chain->getFirst(); info; info = info->getNext())
            info->print(fe, pOutFile, indent+40);
         break;
      }

   }

int32_t TR::SwitchAnalyzer::countMajorsInChain(TR_LinkHead<SwitchInfo> *chain)
   {
   if (!chain) return 0;
   int32_t numNonUnique = 0;
   int32_t numUnique    = 0;
   for (SwitchInfo *info = chain->getFirst(); info; info = info->getNext())
      {
      if (info->_kind != Unique)
         numNonUnique++;
      else
         numUnique++;
      }
   return 2 * numNonUnique + numUnique;
   }

TR::SwitchAnalyzer::SwitchInfo *TR::SwitchAnalyzer::getLastInChain(TR_LinkHead<SwitchInfo> *chain)
   {
   if (!chain) return 0;
   SwitchInfo *info = chain->getFirst();
   if (!info) return 0;
   while (info->getNext())
      info = info->getNext();
   return info;
   }

TR::Block *TR::SwitchAnalyzer::peelOffTheHottestValue(TR_LinkHead<SwitchInfo> *chain)
   {
   if (!_haveProfilingInfo)
      return NULL;

   if (!chain)
      return NULL;

   printInfo(comp()->fe(), comp()->getOutFile(), chain);

   float cutOffFrequency = 0.33f;

   if (trace())
      {
      traceMsg(comp(), "\nLooking to see if we have a value that's more than 33%% of all cases.\n");
      }

   TR_LinkHead<SwitchInfo> *list = chain;
   SwitchInfo *first = chain->getFirst();

   if (first->_kind == Dense)
      list = first->_chain;

   SwitchInfo *cursor = NULL;
   float maxFreq = 0.0f;
   SwitchInfo *topNode = NULL;
   for (cursor = list->getFirst(); cursor; cursor= cursor->getNext())
      {
      if (cursor->_freq >= maxFreq)
         {
         maxFreq = cursor->_freq;
         topNode = cursor;
         }
      }

   if (topNode && (topNode->_kind == Unique) && (maxFreq>cutOffFrequency))
      {
      bool _isInt64 = false;

      if (_switch->getChild(0)->getType().isInt64())
         _isInt64 = true;

      TR::ILOpCodes cmpOp = TR::BadILOp;

      TR::Block *newBlock = NULL;

      cmpOp = _isInt64 ? (_signed ? TR::iflcmpeq : TR::iflucmpeq) : (_signed ? TR::ificmpeq : TR::ifiucmpeq);
      newBlock = addIfBlock(cmpOp, topNode->_min, topNode->_target);

      if (trace())
         {
         traceMsg(comp(), "Found a dominant entry in a dense node for target 0x%p with frequency of %f.\n", topNode->_target->getNode(), maxFreq);
         traceMsg(comp(), "Peeling off a quick test for this entry.\n");
         }

      return newBlock;
      }

   return NULL;
}

TR::Block *TR::SwitchAnalyzer::checkIfDefaultIsDominant(SwitchInfo *start)
   {
   if (!_haveProfilingInfo)
      return NULL;

   if (!start)
      return NULL;

   bool hasChildWithDecentFrequency = false;
   int32_t numCases      = _switch->getNumChildren() - 2;
   float cutOffFrequency = .5f / ((float)numCases);

   if (trace())
      {
      traceMsg(comp(), "Looking to see if the default case is dominant. Number of cases is %d, cut off frequency set to %f\n", numCases, cutOffFrequency);
      }

   for (SwitchInfo *temp = start;temp; temp = temp->getNext())
      {
      if (temp->_freq >= cutOffFrequency)
         {
         hasChildWithDecentFrequency = true;
         if (trace())
            {
            traceMsg(comp(), "Found child with frequency of %f. The default case isn't that dominant.\n", temp->_freq);
            }
         break;
         }
      }

   if (!hasChildWithDecentFrequency)
      {
      int64_t  absMin = start->_min;
      int64_t  absMax = start->_max;

      if (trace())
         {
         traceMsg(comp(), "The default case is dominant, we'll generate the range tests.\n");
         }

      for (SwitchInfo *temp = start->getNext();temp; temp = temp->getNext())
         {
         if (absMin > temp->_min)
            absMin = temp->_min;
         if (absMax < temp->_max)
            absMax = temp->_max;
         }

      if (trace())
         {
         traceMsg(comp(), "Range [%d, %d]\n", absMin, absMax);
         }

      bool _isInt64 = false;
      if (_switch->getChild(0)->getType().isInt64())  _isInt64 = true;

      TR::ILOpCodes cmpOp = TR::BadILOp;

      TR::Block *newBlock = NULL;
      cmpOp = _isInt64 ? (_signed ? TR::iflcmplt : TR::iflucmplt) : (_signed ? TR::ificmplt : TR::ifiucmplt);
      addIfBlock(cmpOp, absMin, _defaultDest);
      cmpOp = _isInt64 ? (_signed ? TR::iflcmpgt : TR::iflucmpgt) : (_signed ? TR::ificmpgt : TR::ifiucmpgt);
      newBlock = addIfBlock(cmpOp, absMax, _defaultDest);

      return newBlock;
      }

   return NULL;
   }

// If unsigned, break consecutive range that straddles 0, TR::getMaxUnsigned<TR::Int32>(), and sort by unsigned order.
// todo: join range/dense that straddle TR::getMaxSigned<TR::Int32>(),TR::getMaxSigned<TR::Int32>()+1
// Assumes that it's in signed sorted order
/*
 * [-110,-100]           [0,10]
 * [-10,10]       ==>    [100,110]
 * [100,110]             [-110,-100]
 *                       [-10,-1]
 */
void TR::SwitchAnalyzer::fixUpUnsigned(TR_LinkHead<SwitchInfo> *chain)
   {
   SwitchInfo *csr = chain->getFirst();
   if (!csr)
      return;

   if (csr->_min >= 0)
      return;  // nothing to do

   SwitchInfo *lastNeg = NULL;
   SwitchInfo *firstPos = NULL;
   for(;; csr = csr->getNext())
      {
      if (csr->_max >= 0 && csr->_min < 0)
         {
         // break up zero straddler
         SwitchInfo *next = csr->getNext();
         firstPos = new (trStackMemory()) SwitchInfo(*csr);
         firstPos->_max = csr->_max;
         SwitchInfo *firstPosFirstChainInfo = NULL, *prev = NULL;
         for (auto info = firstPos->_chain->getFirst(); info; info = info->getNext())
            {
            if (info->_min >= 0 || info->_max >= 0)
               {
               firstPosFirstChainInfo = info;
               if (prev)
                  prev->setNext(NULL);
               break;
               }
            prev = info;
            }
         TR_ASSERT(firstPosFirstChainInfo != NULL, "Could not find the neg to pos SwitchInfo\n");
         firstPos->_chain = new (trStackMemory()) TR_LinkHead<SwitchInfo>();
         firstPos->_chain->setFirst(firstPosFirstChainInfo);
         firstPos->_min = firstPosFirstChainInfo->_min;

         lastNeg = csr;
         lastNeg->_max = prev ? prev->_max : -1;
         lastNeg->setNext(NULL);

         csr = firstPos;  // continue searching positive chain until tail
         }
      else if(firstPos == NULL && csr->_min >= 0)
        {
        firstPos = csr;
        lastNeg->setNext(NULL);
        }
      else if(csr->_max < 0)
        lastNeg = csr;

      if (!csr->getNext()) // stop if last node
         break;
      }

   // set firstPos as first in the chain and replace NULL after csr with firstNeg.
   // [pos range ... ->csr->NULL] -> [neg range]
   if (lastNeg && firstPos)
      {
      SwitchInfo *firstNeg = chain->getFirst();
      chain->setFirst(firstPos);
      csr->setNext(firstNeg);
      }
   }

void TR::SwitchAnalyzer::emit(TR_LinkHead<SwitchInfo> *chain, TR_LinkHead<SwitchInfo> *bound, TR_LinkHead<SwitchInfo> *earlyUniques)
   {
   int32_t majorsInChain = countMajorsInChain(chain);
   int32_t majorsInBound = countMajorsInChain(bound);
   int32_t majorsInEarly = countMajorsInChain(earlyUniques);
   int32_t numCases      = _switch->getCaseIndexUpperBound() - 2;
   int32_t numMajors     = majorsInChain + majorsInBound + majorsInEarly;

   if (_switch->getOpCodeValue() == TR::lookup && (!comp()->isOptServer() || numCases>LOOKUP_SWITCH_GEN_IN_IL_OVERRIDE))
      {
      if (trace())
         traceMsg(comp(),"numMajors %d, majorsInBound %d, numCases %d\n", numMajors, majorsInBound, numCases);

      // if the number of cases is so small that it's always better to convert the switch to ifs, skip checks for backing out
      if (numCases > SWITCH_TO_IFS_THRESHOLD)
         {
         // Do the tranformation only if we know that the search space is decreasing in size
         if (4 * numMajors > 3 * numCases) // if nummajors must be no more than 3/4th of numcases
            return;
	     if (numCases < MIN_CASES_FOR_OPT) // if numcases is small
            return;
         // Do the transformation only if we know that the primary chain contains most
         // of the cases (majorsInBound == numCasesInBound, since bound does not have range
         // or dense nodes)
         if (majorsInBound * 3 > numCases) // bound-cases must be no more than a third of the toal cases
            return;
         }
      }

   if (!performTransformation(comp(), "%soptimized switch in block_%d\n", OPT_DETAILS, _block->getNumber()))
      return;

   // Check if CannotOverflow flag on switch should be propagated
   // This flag indicates that the possible values of the switch operand are withing the min max range of all the case statements
   bool keepOverflow=true;
   if(majorsInBound!=0 || majorsInEarly!=0 || !_switch->chkCannotOverflow())
     keepOverflow=false;
   SwitchInfo *info=chain->getFirst();
   if(info==NULL || info->getNext()!=NULL || info->_kind != Dense)
     keepOverflow=false;
   if(keepOverflow==false)
     _switch->setCannotOverflow(false); // Cancel the unneeded range check
   else if(!performTransformation(comp(), "%sUnneeded range check on switch propagated\n", OPT_DETAILS))
     _switch->setCannotOverflow(false); // Cancel the unneeded range check if optimization limit reached


   // check if some element needs to be extracted because it is high frequency
   // we can pull them out and test them first before searching the rest of the chain
   //
   // log(numMajors in range) * (probability of range) > 2 for ranges to be extracted
   // log(numMajors in dense) * (probability of dense) > 2 for denses to be extracted
   // uniques cannot be extracted since they don't exist in the primary chain
   //
   // extracting multiple elements will almost never be practical unless they have very
   // high probablity (first = 60%, second = 30%, etc.)
   //
   // FIXME: implement the above
   //
   // We now have a list of elements, earlyUniques, that we want to check early. When the above is
   // implemented, we can simply add the items to this list; code below issues
   // them before anything else.

   //FIXME: the following code doesn't seem to be too graceful for unsigned values
   CASECONST_TYPE rangeLeft, rangeRight;

   switch (_switch->getChild(0)->getDataType())
      {
      case TR::Int16:
         rangeLeft  = _signed ? TR::getMinSigned<TR::Int16>() : TR::getMinUnsigned<TR::Int16>();
         rangeRight = _signed ? TR::getMaxSigned<TR::Int16>() : TR::getMaxUnsigned<TR::Int16>();
      default:
         rangeLeft  = TR::getMinSigned<TR::Int32>();
         rangeRight = TR::getMaxSigned<TR::Int32>();
         break;
      }

   _temp = comp()->getSymRefTab()->
      createTemporary(comp()->getMethodSymbol(), _isInt64 ? TR::Int64 : TR::Int32);

   TR::Block *newBlock = 0;
   if (majorsInBound > 0)
      {
      if (majorsInBound <= MIN_SIZE_FOR_BIN_SEARCH)
         {
         newBlock = linearSearch(bound->getFirst());
         if (comp()->isOptServer() &&
             _switch->getOpCodeValue() != TR::lookup)
            {
            TR::Block *peeledOffBlock = peelOffTheHottestValue(bound);
            if (peeledOffBlock)
               newBlock = peeledOffBlock;
            }
         }
      else
         {
         newBlock = binSearch(bound->getFirst(), getLastInChain(bound), majorsInBound,
                              rangeLeft, rangeRight);
         if (comp()->isOptServer())
            {
            TR::Block *defaultNewBlock = checkIfDefaultIsDominant(bound->getFirst());
            if (defaultNewBlock)
               newBlock = defaultNewBlock;
            }
         }

      TR_ASSERT(newBlock == _nextBlock, "assertion failure");
      _defaultDest = newBlock->getEntry();
      }

   if (majorsInChain > 0)
      {
      if (majorsInChain <= MIN_SIZE_FOR_BIN_SEARCH)
         {
         newBlock = linearSearch(chain->getFirst());
         if (comp()->isOptServer() &&
             _switch->getOpCodeValue() != TR::lookup)
            {
            TR::Block *peeledOffBlock = peelOffTheHottestValue(chain);
            if (peeledOffBlock)
               newBlock = peeledOffBlock;
            }
         }
      else
         {
         newBlock = binSearch(chain->getFirst(), getLastInChain(chain), majorsInChain,
                              rangeLeft, rangeRight);
         if (comp()->isOptServer())
            {
            TR::Block *defaultNewBlock = checkIfDefaultIsDominant(chain->getFirst());
            if (defaultNewBlock)
               newBlock = defaultNewBlock;
            }
         }
      _defaultDest = newBlock->getEntry();
      }

   if (majorsInEarly > 0)
      {
      if (majorsInEarly <= MIN_SIZE_FOR_BIN_SEARCH)
         newBlock = linearSearch(earlyUniques->getFirst());
      else
         newBlock = binSearch(earlyUniques->getFirst(), getLastInChain(earlyUniques), majorsInEarly,
                              rangeLeft, rangeRight);
      }

   TR_ASSERT(newBlock, "At least one of primary, early unique, and bound chains must not be empty");
   _cfg->addEdge(_block, newBlock);
   TR::Node *store = TR::Node::createStore(_temp, _switch->getChild(0));
   _block->append(TR::TreeTop::create(comp(), store));
   TR::TransformUtil::removeTree(comp(), _switchTree);

   for (auto edge = _block->getSuccessors().begin(); edge != _block->getSuccessors().end();)
      {
      if ((*edge)->getTo() != newBlock)
         _cfg->removeEdge(*(edge++));
      else
    	 edge++;
      }
   }
TR::Block *TR::SwitchAnalyzer::binSearch(SwitchInfo *startNode, SwitchInfo *endNode, int32_t numMajors, CASECONST_TYPE rangeLeft, CASECONST_TYPE rangeRight)
   {
   TR_ASSERT(numMajors > 0, "majors must be > 0");
   TR::ILOpCodes cmpOp = TR::BadILOp;

   // Unique Left
   //
   if (numMajors == 1)
      {
      TR_ASSERT(startNode == endNode && startNode->_kind == Unique, "illegal leaf node");

      if (rangeLeft == rangeRight)
         return addGotoBlock(endNode->_target);
      else
         {
         addGotoBlock(_defaultDest);
         cmpOp = _isInt64 ? (_signed ? TR::iflcmpeq : TR::iflucmpeq) : (_signed ? TR::ificmpeq : TR::ifiucmpeq);
         return addIfBlock  (cmpOp, endNode->_max, endNode->_target);
         }
      }

   // Range or Dense Leafs
   //
   if (numMajors == 2 && startNode == endNode)
      {
      TR_ASSERT(startNode->_kind != Unique, "cannot have two majors in a unique leaf");

      if (endNode->_kind == Range)
         {
         if (rangeRight == endNode->_max && rangeLeft == endNode->_min)
            {
            // Both bound tests can be omitted
            //
            return addGotoBlock(endNode->_target);
            }
         else if (rangeRight == endNode->_max)
            {
            // Upper bound test can be omitted
            //
            addGotoBlock(_defaultDest);
            cmpOp = _isInt64 ? (_signed ? TR::iflcmpge : TR::iflucmpge) : (_signed ? TR::ificmpge : TR::ifiucmpge);
            return addIfBlock  (cmpOp, endNode->_min, endNode->_target);
            }
         else if (rangeLeft  == endNode->_min)
            {
            // Lower bound test can be omitted
            //
            addGotoBlock(_defaultDest);
            cmpOp = _isInt64 ? (_signed ? TR::iflcmple : TR::iflucmple) : (_signed ? TR::ificmple : TR::ifiucmple);
            return addIfBlock  (cmpOp, endNode->_max, endNode->_target);
            }

         // Both bound tests must be done
         //
         addGotoBlock(_defaultDest);

         addIfBlock  (_signed ? TR::ificmpge : TR::ifiucmpge, endNode->_min, endNode->_target);
         cmpOp = _isInt64 ? (_signed ? TR::iflcmpgt : TR::iflucmpgt) : (_signed ? TR::ificmpgt : TR::ifiucmpgt);
         return addIfBlock  (cmpOp, endNode->_max, _defaultDest);
         }
      else
         {
         TR::Block *tableBlock = addTableBlock(endNode);
         if (rangeRight == endNode->_max && rangeLeft == endNode->_min)
            {
            TR::Node *tableNode = tableBlock->getLastRealTreeTop()->getNode();
            tableNode->setIsSafeToSkipTableBoundCheck(true);
            }
         return tableBlock;
         }
      }

   int32_t pivot = numMajors / 2; // FIXME: divide the freq range into two rather than the numMajors

   // Find pivot-th major from startNode
   //
   SwitchInfo *pivotNode = startNode;
   for (int32_t cursor = 1; true ; pivotNode = pivotNode->getNext())
      {
      if (pivotNode->_kind == Unique)
         {
         if (cursor == pivot)
            break;
         }
      else
         {
         if (cursor == pivot)
            {
            pivot++; // increase pivot to include high end
            break;
            }
         cursor++;
         if (cursor == pivot)
            break;
         }
      cursor++;
      }

   CASECONST_TYPE pivotValue = pivotNode->_max;
   TR::Block *right = binSearch(pivotNode->getNext(), endNode,   numMajors - pivot, pivotValue + 1, rangeRight);
   /* left = */      binSearch(startNode,            pivotNode, pivot,             rangeLeft,      pivotValue);

   cmpOp = _isInt64 ? (_signed ? TR::iflcmpgt : TR::iflucmpgt) : (_signed ? TR::ificmpgt : TR::ifiucmpgt);
   return addIfBlock(cmpOp, pivotValue, right->getEntry());
   }

TR::SwitchAnalyzer::SwitchInfo *TR::SwitchAnalyzer::sortedListByFrequency(SwitchInfo *start)
   {
   SwitchInfo *p, *q, *e, *tail;
   int32_t insize, nmerges, psize, qsize;

   if (!start)
      return NULL;

   insize = 1;

   while (true)
      {
      p = start;
      start = NULL;
      tail = NULL;

      nmerges = 0;

      while (p)
         {
         nmerges++;

         q = p;
         psize = 0;

         for (int32_t i = 0; i < insize; i++)
            {
            psize++;
            q = q->getNext();
            if (!q)
               break;
            }

         qsize = insize;

         while (psize > 0 || (qsize > 0 && q))
            {
            if (psize == 0)
               {
               e = q;
               q = q->getNext();
               qsize--;
               }
            else if (qsize == 0 || !q)
               {
               e = p;
               p = p->getNext();
               psize--;
               }
            else if (p->_freq < q->_freq)
               {
               e = p;
               p = p->getNext();
               psize--;
               }
            else
               {
               e = q;
               q = q->getNext();
               qsize--;
               }

            if (tail)
               {
               tail->setNext(e);
               }
            else
               {
               start = e;
               }

            tail = e;
            }

         p = q;
         }

      tail->setNext(NULL);

      if (nmerges <= 1)
         return start;

      insize *= 2;
      }
   }

TR::Block *TR::SwitchAnalyzer::linearSearch(SwitchInfo *start)
   {
   // FIXME: use profiling info
   //
   TR::Block *newBlock = addGotoBlock(_defaultDest);
   bool _isInt64 = false;
   if (_switch->getChild(0)->getType().isInt64())  _isInt64 = true;

   TR::ILOpCodes cmpOp = TR::BadILOp;

   if ((_switch->getOpCodeValue() == TR::lookup) && trace())
      {
      traceMsg(comp(), "Laying down linear search sequence. Initial switch values order:\n");
      for (SwitchInfo *temp = start;temp; temp = temp->getNext())
         {
         traceMsg(comp(), "0x%p ", temp->_target->getNode());
         }
      traceMsg(comp(), "\n");
      }

   // we sort in ascending order because the loop below
   // prepends the if blocks
   SwitchInfo *cursor = (comp()->isOptServer() &&
                         _switch->getOpCodeValue() == TR::lookup) ? sortedListByFrequency(start) : start;

   if ((_switch->getOpCodeValue() == TR::lookup) && trace())
      {
      traceMsg(comp(), "Ascending sorted order by frequency:\n");
      for (SwitchInfo *temp = cursor;temp; temp = temp->getNext())
         {
         traceMsg(comp(), "0x%p ", temp->_target->getNode());
         }
      traceMsg(comp(), "\n");
      }

   for (;cursor; cursor = cursor->getNext())
      {
      if (cursor->_kind == Unique)
         {
         cmpOp = _isInt64 ? (_signed ? TR::iflcmpeq : TR::iflucmpeq) : (_signed ? TR::ificmpeq : TR::ifiucmpeq);
         newBlock = addIfBlock(cmpOp, cursor->_min, cursor->_target);
         }
      else if (cursor->_kind == Range)
         {

         cmpOp = _isInt64 ? (_signed ? TR::iflcmple : TR::iflucmple) : (_signed ? TR::ificmple : TR::ifiucmple);
         newBlock = addIfBlock(cmpOp, cursor->_max, cursor->_target);
         cmpOp = _isInt64 ? (_signed ? TR::iflcmplt : TR::iflucmplt) : (_signed ? TR::ificmplt : TR::ifiucmplt);
         newBlock = addIfBlock(cmpOp, cursor->_min, _defaultDest);
         }
      else
         {
         newBlock = addTableBlock(cursor);
         }
      _defaultDest = newBlock->getEntry();
      }

   return newBlock;
   }

TR::Block *TR::SwitchAnalyzer::addGotoBlock(TR::TreeTop *dest)
   {
   // assumes that _block does not fall into _nextBlock

   TR::Node *node = TR::Node::create(_switch, TR::Goto);
   node->setBranchDestination(dest);
   TR::Block *newBlock = TR::Block::createEmptyBlock(node, comp(), dest->getNode()->getBlock()->getFrequency(), _block);
   newBlock->append(TR::TreeTop::create(comp(), node));

   _cfg->addNode(newBlock, _block->getParentStructureIfExists(_cfg));
   _cfg->addEdge(newBlock, dest->getNode()->getBlock());

   _block->getExit()->join(newBlock->getEntry());



   if (_nextBlock)
      newBlock->getExit()->join(_nextBlock->getEntry());
   else
      newBlock->getExit()->setNextTreeTop(NULL);


   _nextBlock = newBlock;
   _blocksGeneratedByMe->set(newBlock->getNumber());
   return newBlock;
   }
TR::Block *TR::SwitchAnalyzer::addIfBlock(TR::ILOpCodes opCode, CASECONST_TYPE val, TR::TreeTop *dest)
   {
   TR::Node *constNode = TR::Node::create(_switch, _isInt64 ? (_signed ? TR::lconst : TR::luconst) : (_signed ? TR::iconst : TR::iuconst), 0);
   constNode->set64bitIntegralValue(val);
   TR::Node *node = TR::Node::createif(opCode,
                                     TR::Node::createLoad(_switch, _temp),
                                     constNode);

   node->setBranchDestination(dest);
   TR::Block *newBlock = TR::Block::createEmptyBlock(node, comp(), _block->getFrequency(), _block);

   newBlock->append(TR::TreeTop::create(comp(), node));

   _cfg->addNode(newBlock, _block->getParentStructureIfExists(_cfg));
   _cfg->addEdge(newBlock, dest->getNode()->getBlock());
   _cfg->addEdge(newBlock, _nextBlock);



   _block->getExit()->join(newBlock->getEntry());
   newBlock->getExit()->join(_nextBlock->getEntry());

   _nextBlock = newBlock;
   _blocksGeneratedByMe->set(newBlock->getNumber());


   return newBlock;
   }

TR::Block *TR::SwitchAnalyzer::addTableBlock(SwitchInfo *dense)
   {
   TR_ASSERT(dense->_kind == Dense, "expecting dense node");

   CASECONST_TYPE upperBound = dense->_max - dense->_min;

   TR::SymbolReference *branchTableSymRef = NULL;

   int32_t branchTable = 0;

   TR::Node *node = TR::Node::create(_switch, TR::table, 3 + upperBound + branchTable);
   if(_switch && _switch->chkCannotOverflow())
     node->setCannotOverflow(true); // Pass on info to code gen that table will have all cases covered and not use default case

   if (_signed)
      node->setAndIncChild(0, TR::Node::create(TR::isub, 2,
                                           (_isInt64 ? TR::Node::create(TR::l2i, 1, TR::Node::createLoad(_switch, _temp)) : TR::Node::createLoad(_switch, _temp)),
                                           TR::Node::create(_switch, TR::iconst, 0, dense->_min)));
   else
      node->setAndIncChild(0, TR::Node::create(TR::iusub, 2,
                                           (_isInt64 ? TR::Node::create(TR::l2i, 1, TR::Node::createLoad(_switch, _temp)) : TR::Node::createLoad(_switch, _temp)),
                                           TR::Node::create(_switch, TR::iuconst, 0, dense->_min)));

   node->setAndIncChild(1, TR::Node::createCase(_switch, _defaultDest));

   TR_BitVector seenSuccessors(_cfg->getNextNodeNumber(), trMemory(), stackAlloc);

   TR::Block *newBlock = TR::Block::createEmptyBlock(node, comp(), _block->getFrequency(), _block);

   newBlock->append(TR::TreeTop::create(comp(), node));
   _cfg->addNode(newBlock, _block->getParentStructureIfExists(_cfg));
   _cfg->addEdge(newBlock, _defaultDest->getNode()->getBlock());
   seenSuccessors.set(_defaultDest->getNode()->getBlock()->getNumber());

   _block->getExit()->join(newBlock->getEntry());
   newBlock->getExit()->join(_nextBlock->getEntry());


   SwitchInfo *currentInfo = dense->_chain->getFirst();
   for (int32_t caseIndex = 0;
        caseIndex <= upperBound;
        ++caseIndex)
      {
      TR::TreeTop *dest;
      if (currentInfo && (currentInfo->_min - dense->_min) == caseIndex)
         {
         dest = currentInfo->_target;
         TR::Block *destBlock = dest->getNode()->getBlock();
         if (!seenSuccessors.isSet(destBlock->getNumber()))
            {
            _cfg->addEdge(newBlock, destBlock);
            seenSuccessors.set(destBlock->getNumber());
            }
         currentInfo = currentInfo->getNext();
         }
      else
         dest = _defaultDest;

      node->setAndIncChild(2 + caseIndex, TR::Node::createCase(_switch, dest, caseIndex));
      }

   if (branchTable)
      {
      node->setAndIncChild(2 + upperBound + 1, TR::Node::createWithSymRef(_switch, TR::loadaddr, 0, branchTableSymRef));
      }

   _nextBlock = newBlock;
   _blocksGeneratedByMe->set(newBlock->getNumber());
   return newBlock;
   }


int32_t *TR::SwitchAnalyzer::setupFrequencies(TR::Node *node)
   {
   if (!_haveProfilingInfo) return 0;

   int8_t *targetCounts = (int8_t*)   trMemory()->allocateStackMemory(_cfg->getNextNodeNumber() * sizeof(int8_t));
   memset (targetCounts, 0, sizeof(int8_t) * _cfg->getNextNodeNumber());
   int32_t *frequencies = (int32_t *) trMemory()->allocateStackMemory(node->getCaseIndexUpperBound() * sizeof(int32_t));
   memset  (frequencies, 0, sizeof(int32_t) * node->getCaseIndexUpperBound());

   // Count the number of cases reaching a particular target, divide target block frequency
   // by the number of cases
   //
   CASECONST_TYPE i;
   for (i = node->getCaseIndexUpperBound() - 1; i > 0; --i)
      {
      TR::Node *caseNode = node->getChild(i);
      TR::Block *targetBlock = caseNode->getBranchDestination()->getNode()->getBlock();
      targetCounts[targetBlock->getNumber()]++;
      }

   for (i = node->getCaseIndexUpperBound() - 1; i > 0; --i)
      {
      TR::Node *caseNode = node->getChild(i);
      TR::Block *targetBlock = caseNode->getBranchDestination()->getNode()->getBlock();
      int32_t targetCount = targetCounts[targetBlock->getNumber()];
      TR_ASSERT(targetCount != 0, "unreachle successor of switch statement");
      int32_t frequency = targetBlock->getFrequency() / targetCount;
      frequencies[i] = frequency;

      if (trace())
         traceMsg(comp(), "Switch analyser: Frequency at pos %d is %d\n", i, frequencies[i]);
      }

   // For each case value, lists the frequency of the selector being of that value
   // Note that the default is at frequencies[1], frequencies[0] will always be null.
   //
   return frequencies;
   }

// If true, this case is kept as a unique, and won't be merged into a range or a
// dense set
bool TR::SwitchAnalyzer::keepAsUnique(SwitchInfo *info, int32_t itemNumber)
   {
   return false;
   }

